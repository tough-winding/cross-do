#!/usr/bin/env python3
"""Real concurrent finance transactions against the isolated development stack.

Run after sourcing the test stack.env and building C. Two temporary DBOP servers
share the isolated database. A third connection holds the project lock until
both requests are observed in InnoDB's lock-wait table, so this tests concurrent
database transactions rather than one event-loop server's queued HTTP requests.
Only this suite's new volunteer is bootstrapped through SQL; monetary writes go
through C's service-authenticated API. No external payment gateway is contacted.
The temporary servers/configuration are removed on success or failure. Lock
observation supports MariaDB (INNODB_LOCK_WAITS) and MySQL 8 (data_lock_waits);
the test database user needs permission to inspect those diagnostic tables.
"""
import concurrent.futures
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import select
import socket
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import uuid

from release_integration import ReleaseSuite
from workflow_integration import Client, Failure, options_from_environment, require


ROOT = Path(__file__).resolve().parents[2]


class FinanceSuite(ReleaseSuite):
    def mysql(self):
        o=self.options
        require(bool(o.mysql_database) and 'test' in o.mysql_database.lower(), 'isolated test database required')
        cmd=[o.mysql_binary,'--no-defaults','--batch','--skip-column-names','--unbuffered',
             '--user='+o.mysql_user,'--database='+o.mysql_database]
        cmd += ['--protocol=socket','--socket='+o.mysql_socket] if o.mysql_socket else [
            '--protocol=tcp','--host='+o.mysql_host,'--port='+str(o.mysql_port)]
        env=os.environ.copy()
        if o.mysql_password is not None:env['MYSQL_PWD']=o.mysql_password
        return cmd,env

    @contextlib.contextmanager
    def peers(self):
        config=Path(os.environ.get('CROSSDO_TEST_DBOP_CONFIG','/tmp/crossdo-review-stack/dbop/config/db_config.yaml')).read_text()
        database=re.search(r'^user_database_name:\s*"([^"\n]+)"\s*$',config,re.M)
        require(database is not None and database.group(1)==self.options.mysql_database,
                'temporary DBOP configuration must target the selected isolated test database')
        binary=Path(os.environ.get('CROSSDO_TEST_DBOP_BINARY',str(ROOT/'build/dbop')))
        require(binary.is_file(),'build C DBOP before running finance concurrency tests')
        processes=[]
        with tempfile.TemporaryDirectory(prefix='crossdo-finance-concurrency-') as directory:
            directory=Path(directory)
            try:
                clients=[]
                for index in range(2):
                    with socket.socket() as probe:
                        probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
                    log=directory/('dbop-'+str(index)+'.log')
                    logging=directory/('zlog-'+str(index)+'.conf')
                    logging.write_text('[global]\nstrict init = true\n[formats]\nstd = "%d %V %m%n"\n[rules]\n*.INFO "'+str(log)+'"; std\n')
                    changed=re.sub(r'^server_port:.*$', 'server_port: "'+str(port)+'"',config,flags=re.M)
                    changed=re.sub(r'^log_file:.*$', 'log_file: "'+str(log)+'"',changed,flags=re.M)
                    changed=re.sub(r'^log_config:.*$', 'log_config: "'+str(logging)+'"',changed,flags=re.M)
                    path=directory/('dbop-'+str(index)+'.yaml');path.write_text(changed);path.chmod(0o600)
                    process=subprocess.Popen([str(binary),str(path)],cwd=str(directory),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    processes.append(process)
                    deadline=time.monotonic()+10
                    while True:
                        require(process.poll() is None,'temporary DBOP exited during startup')
                        try:
                            with socket.create_connection(('127.0.0.1',port),timeout=.2):break
                        except OSError:
                            require(time.monotonic()<deadline,'temporary DBOP did not start');time.sleep(.05)
                    clients.append(Client('http://127.0.0.1:'+str(port),self.options.timeout))
                yield clients
            finally:
                for process in processes:
                    if process.poll() is None:process.terminate()
                for process in processes:
                    try:process.wait(timeout=5)
                    except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)

    def call(self,client,route,body):
        return client.request('/workflow/finance/'+route,body,headers={
            'ServiceName':self.options.service_name,'Authorization':self.options.dbop_token})

    def ok_call(self,client,route,body):
        status,value=self.call(client,route,body)
        require(status==200,'finance '+route+' expected 200, received '+str(status));return value

    def settle_callbacks(self,peers,bodies,results):
        # MariaDB's optional snapshot-isolation mode may abort a waiter with
        # ER_CHECKREAD (1020). The API returns 409 and the same event is safe to
        # retry. MySQL's usual current-read behavior accepts both immediately.
        settled=[]
        for index,(status,result) in enumerate(results):
            require(status in (200,409),'concurrent callback must succeed or return retryable 409, received '+str(status))
            if status==409:
                status,result=self.call(peers[index],'complete',bodies[index])
            require(status==200 and result['accepted'],'original event retry must be accepted after concurrent transaction commits')
            settled.append((status,result))
        return settled

    def blocked_race(self,peers,pid,route,bodies):
        maria='MariaDB' in self.sql('SELECT VERSION();')
        command,environment=self.mysql()
        locker=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                                text=True,bufsize=1,env=environment)
        try:
            locker.stdin.write("START TRANSACTION; SELECT project_id FROM project WHERE project_id='{}' FOR UPDATE; SELECT CONNECTION_ID();\n".format(str(uuid.UUID(pid))))
            locker.stdin.flush()
            require(select.select([locker.stdout],[],[],self.options.timeout)[0],'project lock acquisition timed out')
            require(locker.stdout.readline().strip()==pid,'project lock fixture did not find its own project')
            connection=int(locker.stdout.readline().strip())
            barrier=threading.Barrier(2)
            def invoke(index):
                barrier.wait(timeout=self.options.timeout)
                return self.call(peers[index],route,bodies[index])
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as executor:
                futures=[executor.submit(invoke,index) for index in range(2)]
                try:
                    deadline=time.monotonic()+min(8,self.options.timeout-2)
                    while True:
                        observer=("SELECT COUNT(DISTINCT w.requesting_trx_id) FROM information_schema.INNODB_LOCK_WAITS w JOIN information_schema.INNODB_TRX b ON b.trx_id=w.blocking_trx_id WHERE b.trx_mysql_thread_id={};" if maria else
                                  "SELECT COUNT(DISTINCT w.REQUESTING_ENGINE_TRANSACTION_ID) FROM performance_schema.data_lock_waits w JOIN performance_schema.threads b ON b.THREAD_ID=w.BLOCKING_THREAD_ID WHERE b.PROCESSLIST_ID={};")
                        waiting=int(self.sql(observer.format(connection)))
                        if waiting>=2:break
                        require(not any(future.done() for future in futures),'financial request returned before the held project lock')
                        if time.monotonic()>=deadline:
                            states=self.sql("SELECT COALESCE(STATE,'idle'),COUNT(*) FROM information_schema.PROCESSLIST WHERE DB=DATABASE() GROUP BY STATE;")
                            raise Failure('both finance transactions must wait on the project lock (observed '+str(waiting)+'; states '+states.replace('\n','; ')+')')
                        time.sleep(.05)
                finally:
                    locker.stdin.write('COMMIT;\n');locker.stdin.flush()
                return [future.result(timeout=self.options.timeout) for future in futures]
        finally:
            if locker.poll() is None:
                try:locker.communicate('ROLLBACK;\n',timeout=5)
                except subprocess.TimeoutExpired:locker.kill();locker.communicate(timeout=5)

    def project(self,patient,reviewer):
        self.action('application/submit',self.application_payload(),patient)
        key={'user_id':patient['user_id']}
        self.action('application/claim',key,reviewer)
        self.action('application/review',dict(key,decision='approve',note='金融并发测试'),reviewer)
        pid=self.action('project/create',{},patient)['project_id']
        self.action('project/submit',{'project_id':pid,'donation_pool_limit':'1000.00',
                    'pathography':[{'content':'金融并发测试项目','attachments':[]}]},patient)
        self.action('project/claim',{'project_id':pid},reviewer)
        self.action('project/review',{'project_id':pid,'decision':'approve','note':'资料核实'},reviewer)
        return pid

    @staticmethod
    def event(record,actor,event_id=None,status='succeeded',kind='payment'):
        body={'event_id':event_id or 'concurrent:'+str(uuid.uuid4()),'type':kind,'reference_id':record['id'],
              'status':status,'amount':record['amount'],'currency':'CNY','mode':record['mode']}
        body['payload_hash']=hashlib.sha256(json.dumps(body,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        body['actor_id']=actor['user_id'];return body

    def assert_ledger(self,pid,amount,count):
        result=self.sql("SELECT COALESCE(SUM(amount),0),COUNT(*) FROM fund_ledger WHERE project_id='{}';".format(str(uuid.UUID(pid))))
        value,entries=result.split('\t')
        from decimal import Decimal
        require(Decimal(value)==Decimal(amount) and int(entries)==count,'ledger amount and entry count must remain exact')

    def run(self):
        require(bool(self.options.dbop_token),'service token is required for the C boundary concurrency tests')
        require(self.api.ok('/api/payments/settings',method='GET')['mode']=='development','explicit development payment mode required')
        donor=self.login(self.register(2));patients=[self.login(self.register(3)) for _ in range(2)]
        reviewer=self.register(5);self.certify_test_volunteers([reviewer]);self.login(reviewer)
        pid=self.project(patients[0],reviewer);gateway_pid=self.project(patients[1],reviewer)
        order=self.api.ok('/api/donations',{'project_id':pid,'amount':'100.01','idempotency_key':str(uuid.uuid4())},donor['token'])
        with self.peers() as peers:
            event=self.event(order,donor)
            results=self.blocked_race(peers,pid,'complete',[event,event])
            results=self.settle_callbacks(peers,[event,event],results)
            require(all(status==200 and result['accepted'] for status,result in results),'concurrent identical callback must be accepted twice')
            require(sorted(result['duplicate'] for _,result in results)==[False,True],'only one identical event posts money')
            self.assert_ledger(pid,'100.01',1)
            self.passed('same event waits in two real transactions, then posts one ledger entry')

            distinct=[self.event(order,donor),self.event(order,donor)]
            results=self.blocked_race(peers,pid,'complete',distinct)
            results=self.settle_callbacks(peers,distinct,results)
            require(all(status==200 and result['accepted'] and result['duplicate'] for status,result in results),'distinct callbacks on a terminal order must both be accepted without posting')
            self.assert_ledger(pid,'100.01',1)
            self.passed('distinct event IDs for the same terminal payment cannot double-post')

            changed=dict(event,payload_hash='0'*64)
            require(self.call(peers[0],'complete',changed)[0]==409,'conflicting duplicate fingerprint must be rejected')
            mismatch=dict(self.event(order,donor),amount='100.02')
            require(self.call(peers[0],'complete',mismatch)[0]==409,'callback amount mismatch must be rejected')
            require(self.call(peers[0],'complete',self.event(order,donor,status='failed'))[0]==409,'terminal success cannot reverse')
            self.assert_ledger(pid,'100.01',1)
            self.passed('conflicting events, mismatched amounts and terminal reversals leave the journal unchanged')

            refunds=[{'actor_id':donor['user_id'],'id':order['id'],'amount':'80.00','reason':'并发退款测试','idempotency_key':str(uuid.uuid4())} for _ in range(2)]
            results=self.blocked_race(peers,pid,'refund_create',refunds)
            require(sorted(status for status,_ in results)==[200,409],'concurrent outgoing reservations cannot exceed the balance')
            refund=next(value for status,value in results if status==200)
            funds=self.api.ok('/api/projects/'+pid+'/funds',token=patients[0]['token'],method='GET')
            require(funds['balance']=='100.01' and funds['reserved']=='80.00','only winning refund reserves funds')
            failed=self.event(refund,donor,status='failed',kind='refund')
            self.ok_call(peers[0],'complete',failed)
            self.ok_call(peers[1],'complete',failed)
            funds=self.api.ok('/api/projects/'+pid+'/funds',token=patients[0]['token'],method='GET')
            require(funds['balance']=='100.01' and funds['reserved']=='0.00','failed outgoing callback releases reservation once without debit')
            self.assert_ledger(pid,'100.01',1)
            self.passed('concurrent refund limits and repeated failure release are exact')

            refund=self.ok_call(peers[0],'refund_create',dict(refunds[0],amount='100.01',idempotency_key=str(uuid.uuid4())))
            event=self.event(refund,donor,kind='refund')
            results=self.blocked_race(peers,pid,'complete',[event,event])
            results=self.settle_callbacks(peers,[event,event],results)
            require(all(status==200 for status,_ in results),'identical refund callbacks must both succeed')
            self.assert_ledger(pid,'0.00',2)
            funds=self.api.ok('/api/projects/'+pid+'/funds',method='GET')
            require(funds['received']=='100.01' and funds['refunded']=='100.01' and funds['balance']=='0.00' and 'reserved' not in funds,'anonymous gross/refund/net summary must agree')
            self.passed('concurrent refund completion debits once and public totals reconcile')

            create={'actor_id':donor['user_id'],'project_id':pid,'amount':'1.00','mode':'gateway','idempotency_key':str(uuid.uuid4())}
            require(self.call(peers[0],'order_create',create)[0]==409,'zeroed development project cannot accept gateway funds')
            gateway=self.ok_call(peers[0],'order_create',dict(create,project_id=gateway_pid,idempotency_key=str(uuid.uuid4())))
            dev=dict(create,project_id=gateway_pid,mode='development',idempotency_key=str(uuid.uuid4()))
            require(self.call(peers[1],'order_create',dev)[0]==409,'gateway project cannot accept simulated funds')
            self.api.reject('/api/donations/'+gateway['id']+'/simulate-payment',token=donor['token'],allowed=(403,))
            self.ok_call(peers[0],'complete',self.event(gateway,donor,status='failed'))
            self.assert_ledger(gateway_pid,'0.00',0)
            self.passed('development and gateway funds remain isolated in both directions without external payment calls')
        print('Completed {} real finance concurrency groups using {} new accounts.'.format(self.checks,len(self.accounts)))


def main():
    suite=FinanceSuite(options_from_environment())
    try:suite.run()
    except (AssertionError,ValueError,OSError,urllib.error.URLError,subprocess.SubprocessError) as error:
        print('FAIL '+(str(error) if isinstance(error,(Failure,FileNotFoundError)) else type(error).__name__),file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':sys.exit(main())
