CC ?= cc
CFLAGS ?= -O2 -std=c99 -D_POSIX_C_SOURCE=200809L
MYSQL_LIB ?= -lmariadb
DBOP_LIBS = -levent -ljansson -lzlog -lyaml -luuid -lhiredis $(MYSQL_LIB) -lpthread
IAM_LIBS = -lyaml -lhiredis -lcurl -levent -levent_pthreads -lssl -lcrypto -lpthread -lzlog -luuid -ljansson

.PHONY: c web package java-test clean
c: build/dbop build/iam

build/dbop: DBOP/database_operation.c $(wildcard DBOP/workflow_*.h)
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< $(DBOP_LIBS)

build/iam: IAM/identity_access_management.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ $< $(IAM_LIBS)

web:
	cd frontend && npm ci --no-audit --no-fund && npm run build

package: web
	cd java/org.crossdo && mvn test package

java-test:
	cd java/org.crossdo && mvn test

clean:
	rm -rf build
