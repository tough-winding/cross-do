#!/usr/bin/env python3
"""Compare workflow query plans using synthetic data in a disposable MySQL 8 DB.

Requires MySQL >= 8.0.18 (EXPLAIN ANALYZE) and CREATE/DROP DATABASE privileges.
Connection settings match schema_integration.py; no application database is used.
The optimized SQL is extracted from current DBOP source so measurements follow
implementation changes. Baseline SQL is retained below for reproducibility.

Example:
  CROSSDO_TEST_MYSQL_BINARY=mysql python3 tools/tests/query_benchmark.py \
      --rows 100000 --output /tmp/crossdo-query-report.json

All times are server EXPLAIN ANALYZE milliseconds, median after one warmup; they
exclude client startup, HTTP, JSON assembly and network transfer. There are no
hardware-dependent timing assertions. Every created database is cleaned up.
"""

import argparse
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys

from schema_integration import Database, Failure, ROOT, SCHEMA, require, sql_text

ACTOR = '99999999-9999-4999-8999-000000000001'
OTHER = '99999999-9999-4999-8999-000000000002'
APP_COLUMNS = 'a.user_id,u.user_name,u.user_age,u.user_status,a.current_volunteer,a.submitted_at,a.last_change_time'
PROJECT_COLUMNS = 'project_id,user_name,user_age,project_status,current_volunteer,project_create_time,last_update_time'
PUBLIC_COLUMNS = 'project_id,user_name,user_age,project_status,project_create_time,donation_pool_limit,current_donations_received,current_used_donations,last_update_time'
BASELINES = {
    'applications': f'SELECT {APP_COLUMNS} FROM patient_application a JOIN user u ON a.user_id=u.user_id '
        'WHERE u.user_permission=3 AND u.user_id<>? AND (u.user_status IN (2,6) OR '
        '(u.user_status IN (3,7) AND a.current_volunteer=?)) ORDER BY a.submitted_at,a.user_id LIMIT ? OFFSET ?',
    'project_tasks': f'SELECT {PROJECT_COLUMNS} FROM project WHERE user_id<>? AND '
        '(project_status IN (2,6) OR (project_status IN (3,7) AND current_volunteer=?)) '
        'ORDER BY project_create_time,project_id LIMIT ? OFFSET ?',
    'public_projects': f'SELECT {PUBLIC_COLUMNS} FROM project WHERE project_status=0 '
        'ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?',
    'public_entries': 'SELECT e.project_id,e.record FROM project_pathography_entry e JOIN '
        '(SELECT project_id,submission_no FROM project WHERE project_status=0 '
        'ORDER BY project_create_time DESC,project_id DESC LIMIT ? OFFSET ?) p '
        'ON e.project_id=p.project_id AND e.submission_no=p.submission_no ORDER BY e.project_id,e.sequence_no',
}


def source_queries(filename, function):
    text = (ROOT / 'DBOP' / filename).read_text()
    # Limit extraction to the selected function; SQL is deliberately kept as
    # adjacent C string literals in DBOP. Fail explicitly if that contract changes.
    start = text.index('static json_t *' + function + '(')
    end = text.find('\nstatic ', start + 1)
    body = text[start:end if end >= 0 else len(text)]
    pattern = r'wf_query\s*\(\s*ctx\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\s*,\s*(\d+)\s*,\s*params\s*\)'
    found = []
    for match in re.finditer(pattern, body):
        query = ''.join(json.loads(part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', match[1]))
        require(query.count('?') == int(match[2]), 'C query placeholder count changed')
        found.append(query)
    require(bool(found), 'could not extract literal SQL from ' + function)
    return found


def optimized_queries():
    applications = source_queries('workflow_accounts.h', 'wf_application_list')
    tasks = source_queries('workflow_projects.h', 'wf_project_tasks')
    public = source_queries('workflow_projects.h', 'wf_project_list')
    require(len(applications) == 1 and len(tasks) == 1 and len(public) == 3,
            'query layout changed; update benchmark parameter bindings')
    summary = next(query for query in public if ' AS summary ' in query)
    entries = next(query for query in public if query.startswith('SELECT e.project_id,e.record'))
    metadata = next(query for query in public if query not in (summary, entries))
    return dict(applications=applications[0], project_tasks=tasks[0], public_projects=metadata,
                public_entries=entries, public_summary=summary)


def bind(query, values):
    parts = query.split('?')
    require(len(parts) == len(values) + 1, 'benchmark SQL parameter count mismatch')
    result = parts[0]
    for value, part in zip(values, parts[1:]):
        result += (str(value) if isinstance(value, int) else sql_text(value)) + part
    return result


def parameters(name, offset, optimized=False):
    window, limit = offset + 20, 20
    if name == 'applications' and optimized:
        return [ACTOR, window, ACTOR, ACTOR, window, limit, offset]
    if name == 'project_tasks' and optimized:
        return [ACTOR, window, ACTOR, window, ACTOR, ACTOR, window, ACTOR, ACTOR, window, limit, offset]
    if name in ('applications', 'project_tasks'):
        return [ACTOR, ACTOR, limit, offset]
    return [limit, offset]


class Benchmark:
    def __init__(self, options):
        self.options = options
        self.db = Database(options)
        self.database = None
        self.optimized = optimized_queries()
        self.report = {'rows_per_main_table': options.rows, 'completed_fraction': .99,
                       'timestamp_ties': 'five consecutive rows share each timestamp',
                       'history_entries_per_latest_project': options.history_entries,
                       'retained_prior_versions': 2,
                       'warm_measured_runs': options.repeats, 'timing_unit': 'server milliseconds',
                       'timing_scope': 'EXPLAIN ANALYZE; excludes client startup, HTTP and transfer',
                       'cases': [], 'equivalence_checks': 0}

    def execute(self, query):
        return self.db.execute(query, self.database)

    def setup(self):
        self.report['server_version'] = self.db.execute('SELECT VERSION();')
        require(re.match(r'8\.', self.report['server_version']) is not None,
                'this benchmark requires MySQL 8 EXPLAIN ANALYZE format')
        settings = self.db.execute('SELECT @@innodb_buffer_pool_size,@@max_connections;').split('\t')
        self.report['server_settings'] = {'innodb_buffer_pool_size_bytes': int(settings[0]),
                                          'max_connections': int(settings[1])}
        self.database = self.db.create('queries')
        self.db.import_file(SCHEMA, self.database)
        count = self.options.rows
        completed = count * 99 // 100
        actor, other = sql_text(ACTOR), sql_text(OTHER)
        # No source contains personal data, real phones, identities or credentials.
        self.execute(f"""
        SET SESSION cte_max_recursion_depth={count + 1};
        CREATE TABLE bench_numbers(n INT PRIMARY KEY);
        INSERT INTO bench_numbers WITH RECURSIVE seq AS
          (SELECT 1 n UNION ALL SELECT n+1 FROM seq WHERE n<{count}) SELECT n FROM seq;
        INSERT INTO user(user_id,user_permission,user_name,user_status,real_name_authentication,existing_projects)
          VALUES ({actor},5,'synthetic_reviewer',0,1,0),({other},5,'synthetic_other_reviewer',0,1,0);
        INSERT INTO user(user_id,user_permission,user_name,user_status,real_name_authentication,existing_projects,registration_date)
          SELECT CONCAT('00000000-0000-4000-8000-',LPAD(n,12,'0')),3,CONCAT('synthetic_',n),
          CASE WHEN n<={completed} THEN 0 WHEN MOD(n,4)=0 THEN 2 WHEN MOD(n,4)=1 THEN 6 WHEN MOD(n,4)=2 THEN 3 ELSE 7 END,
          IF(n<={completed},1,0),0,DATE_ADD('2020-01-01',INTERVAL FLOOR(n/5) MINUTE) FROM bench_numbers;
        INSERT INTO patient_application(user_id,materials,current_volunteer,previous_volunteers,verification_record,submitted_at,last_change_time)
          SELECT CONCAT('00000000-0000-4000-8000-',LPAD(n,12,'0')),'[]',
          CASE WHEN n>{completed} AND MOD(n,4) IN (2,3) THEN IF(MOD(FLOOR((n-{completed}-1)/4),20)=0,{actor},{other}) ELSE NULL END,
          '[]','[]',DATE_ADD('2020-01-01',INTERVAL FLOOR(n/5) MINUTE),DATE_ADD('2020-01-01',INTERVAL FLOOR(n/5) MINUTE) FROM bench_numbers;
        INSERT INTO project(project_id,user_id,user_name,user_age,project_status,verification_record,previous_volunteers,pathography,
          current_volunteer,question_is_volunteer_assigned,project_create_time,donation_pool_limit,current_donations_received,current_used_donations,last_update_time)
          SELECT CONCAT('11111111-1111-4111-8111-',LPAD(n,12,'0')),CONCAT('00000000-0000-4000-8000-',LPAD(n,12,'0')),
          CONCAT('synthetic_',n),40,
          CASE WHEN n<={completed} THEN 0 WHEN MOD(n,4)=0 THEN 2 WHEN MOD(n,4)=1 THEN 6 WHEN MOD(n,4)=2 THEN 3 ELSE 7 END,
          '[]','[]','[]',CASE WHEN n>{completed} AND MOD(n,4) IN (2,3) THEN IF(MOD(FLOOR((n-{completed}-1)/4),20)=0,{actor},{other}) ELSE NULL END,
          IF(n>{completed} AND MOD(n,4) IN (2,3),1,0),DATE_ADD('2020-01-01',INTERVAL FLOOR(n/5) MINUTE),
          1000.00,10.00,0.00,DATE_ADD('2020-01-01',INTERVAL FLOOR(n/5) MINUTE) FROM bench_numbers;
        INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record)
          SELECT project_id,0,1,JSON_OBJECT('content',RPAD('Synthetic progress ',1000,'x'),
          'attachments',JSON_ARRAY(),'time','2026-01-01 00:00:00') FROM project;
        ANALYZE TABLE user,patient_application,project,project_pathography_entry;
        """)
        print('Prepared synthetic dataset: {} rows per main table.'.format(count), flush=True)

    @staticmethod
    def plan_details(plan):
        number = r'[0-9.eE+-]+'
        root = re.search(r'actual time=(' + number + r')\.\.(' + number + r') rows=(' + number + r') loops=(\d+)', plan)
        require(root is not None, 'unrecognized MySQL EXPLAIN ANALYZE format')
        access = []
        for line in plan.splitlines():
            if re.search(r'(scan|lookup) on ', line, re.I):
                match = re.search(r'actual time=(' + number + r')\.\.(' + number + r') rows=(' + number + r') loops=(\d+)', line)
                if match:
                    access.append({'operator': line.strip().split('  (')[0],
                                   'actual_rows_per_loop': float(match[3]), 'loops': int(match[4]),
                                   'rows_times_loops': float(match[3]) * int(match[4])})
        return float(root[2]), int(float(root[3])), access

    def measure(self, label, query):
        self.execute('EXPLAIN ANALYZE ' + query)  # Warm the plan/data path once.
        measured = []
        for _ in range(self.options.repeats):
            plan = self.execute('EXPLAIN ANALYZE ' + query)
            measured.append(self.plan_details(plan)[0])
        _, rows, access = self.plan_details(plan)
        result = {'name': label, 'median_ms': statistics.median(measured), 'samples_ms': measured,
                  'returned_rows': rows, 'access_nodes': access, 'sql': query, 'plan': plan}
        self.report['cases'].append(result)
        print('{}: {:.3f} ms, {} result rows'.format(label, result['median_ms'], rows), flush=True)
        return result

    def run_queries(self):
        for offset in (0, 17, 200, 10000):
            for name in BASELINES:
                original = bind(BASELINES[name], parameters(name, offset))
                current = bind(self.optimized[name], parameters(name, offset, True))
                before = self.execute(original)
                after = self.execute(current)
                require(before == after, '{} offset {} differs from baseline, including order'.format(name, offset))
                self.report['equivalence_checks'] += 1
                # Exact row equivalence is tested at all offsets; timing focuses
                # on the first page and a deep page to keep the run affordable.
                if offset in (0, 10000):
                    self.measure(name + '_before_offset' + str(offset), original)
                    self.measure(name + '_after_offset' + str(offset), current)
            summary = bind(self.optimized['public_summary'], [20, offset])
            rows = self.execute(summary).splitlines()
            metadata = self.execute(bind(BASELINES['public_projects'], [20, offset])).splitlines()
            require([row.split('\t')[0] for row in rows] == [row.split('\t')[0] for row in metadata],
                    'summary changed project order')
            require(all(len(row.split('\t')[-1]) <= 240 for row in rows), 'summary exceeded preview limit')
            self.report['equivalence_checks'] += 1
            if offset in (0, 10000):
                self.measure('public_summary_offset' + str(offset), summary)

    def long_history(self):
        extra = self.options.history_entries - 1
        if extra:
            self.execute(f"""
            INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record)
            SELECT p.project_id,0,n.n+1,JSON_OBJECT('content',RPAD('Synthetic follow-up ',1000,'x'),
              'attachments',JSON_ARRAY(),'time','2026-01-01 00:00:00')
            FROM (SELECT project_id FROM project WHERE project_status=0
                  ORDER BY project_create_time DESC,project_id DESC LIMIT 20) p
            JOIN bench_numbers n ON n.n<={extra};
            ANALYZE TABLE project_pathography_entry;
            """)
        # Retain two previous submission versions. The final entry query must
        # seek the current (project_id, submission_no), rather than reading every
        # version for each selected project and filtering afterwards.
        for version in (1, 2):
            self.execute(f"""
            INSERT INTO project_pathography_entry(project_id,submission_no,sequence_no,record)
            SELECT e.project_id,{version},e.sequence_no,e.record
            FROM (SELECT project_id FROM project WHERE project_status=0
                  ORDER BY project_create_time DESC,project_id DESC LIMIT 20) p
            JOIN project_pathography_entry e ON e.project_id=p.project_id AND e.submission_no=0;
            """)
        self.execute("""
        UPDATE project p JOIN (SELECT project_id FROM
          (SELECT project_id FROM project WHERE project_status=0
           ORDER BY project_create_time DESC,project_id DESC LIMIT 20) selected_page) page
          ON p.project_id=page.project_id SET p.submission_no=2;
        ANALYZE TABLE project_pathography_entry;
        """)
        full = bind(BASELINES['public_entries'], [20, 0])
        current_full = bind(self.optimized['public_entries'], [20, 0])
        summary = bind(self.optimized['public_summary'], [20, 0])
        full_data, summary_data = self.execute(full), self.execute(summary)
        require(full_data == self.execute(current_full), 'full-history compatibility changed')
        require(len(full_data.splitlines()) == 20 * self.options.history_entries, 'long-history fixture size mismatch')
        require(len(summary_data.splitlines()) == 20, 'summary rows grew with project history')
        self.report['equivalence_checks'] += 1
        self.measure('long_history_full_entries_before', full)
        self.measure('long_history_full_entries_after', current_full)
        self.measure('long_history_summary', summary)
        self.report['long_history_result_bytes'] = {
            'sql_full_entries': len(full_data.encode()), 'sql_summary': len(summary_data.encode()),
            'note': 'UTF-8 tab-separated SQL results, not HTTP response sizes'}

    def run(self):
        self.setup()
        self.run_queries()
        self.long_history()
        print('Validated {} exact-result/order checks.'.format(self.report['equivalence_checks']), flush=True)
        if self.options.output:
            Path(self.options.output).write_text(json.dumps(self.report, ensure_ascii=False, indent=2) + '\n')
            print('Report: ' + str(self.options.output), flush=True)


def options_from_environment():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mysql-binary', default=os.environ.get('CROSSDO_TEST_MYSQL_BINARY', 'mysql'))
    parser.add_argument('--mysql-user', default=os.environ.get('CROSSDO_TEST_MYSQL_USER', 'root'))
    parser.add_argument('--mysql-socket', default=os.environ.get('CROSSDO_TEST_MYSQL_SOCKET'))
    parser.add_argument('--mysql-host', default=os.environ.get('CROSSDO_TEST_MYSQL_HOST', '127.0.0.1'))
    parser.add_argument('--mysql-port', type=int, default=int(os.environ.get('CROSSDO_TEST_MYSQL_PORT', '3306')))
    parser.add_argument('--database-prefix', default='crossdo_query_check')
    parser.add_argument('--timeout', type=int, default=300)
    parser.add_argument('--rows', type=int, default=100000)
    parser.add_argument('--history-entries', type=int, default=500)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--output', type=Path)
    options = parser.parse_args()
    require(re.fullmatch(r'[a-z0-9_]{1,32}', options.database_prefix) is not None, 'invalid database prefix')
    require(1000 <= options.rows <= 1000000, '--rows must be 1000..1000000')
    require(1 <= options.history_entries <= min(options.rows, 2000), '--history-entries must be 1..min(rows,2000)')
    require(1 <= options.repeats <= 20, '--repeats must be 1..20')
    options.mysql_password = os.environ.get('CROSSDO_TEST_MYSQL_PASSWORD')
    return options


def main():
    benchmark = None
    result = 0
    try:
        benchmark = Benchmark(options_from_environment())
        benchmark.run()
    except (AssertionError, OSError, ValueError, StopIteration, subprocess.SubprocessError) as error:
        print('FAIL ' + (str(error) if isinstance(error, Failure) else type(error).__name__), file=sys.stderr)
        result = 1
    finally:
        if benchmark:
            try:
                benchmark.db.cleanup()
            except (AssertionError, OSError, subprocess.SubprocessError):
                print('FAIL temporary database cleanup failed', file=sys.stderr)
                result = 1
    return result


if __name__ == '__main__':
    sys.exit(main())
