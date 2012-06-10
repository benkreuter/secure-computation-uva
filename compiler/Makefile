all: tokens.lex parser.y gate.o deadgates binary identitygates edt-test
	flex tokens.lex
	bison -d parser.y
	flex tokens.lex
	gcc -o parser parser.tab.c lex.yy.c gate.o -lm -O0 -ldb
gate.o: gate.c
	gcc -c gate.c -Wall -Werror
debug:
	gcc -c -g gate.c
	gcc -o parser parser.tab.c lex.yy.c gate.c -lm -g -DDEBUG_ENABLE -ldb
	g++ -c util.cpp -g
	g++ deadgates.cpp util.o gate.o -o deadgates -g -ldb
	g++ binary.cpp util.o gate.o -o binary -g -ldb
	g++ identitygates.cpp util.o gate.o -o identitygates -g -ldb
deadgates: deadgates.o util.o gate.o
	g++ deadgates.o util.o gate.o -o deadgates -O3 -ldb
util.o: util.h util.cpp
	g++ util.cpp -c -Wall -O3
deadgates.o: deadgates.cpp
	g++ deadgates.cpp -c -Wall -O3
binary.o: binary.cpp
	g++ binary.cpp -c -Wall -Werror -O3
binary: binary.o util.o gate.o
	g++ binary.o util.o gate.o -o binary -O3 -ldb
identitygates.o: identitygates.cpp
	g++ identitygates.cpp -c -Wall -Werror -O3
identitygates: identitygates.o util.o gate.o
	g++ identitygates.o util.o gate.o -o identitygates -O3 -ldb
edt.o: edt.cpp
	g++ -c edt.cpp -O3
Bytes.o: Bytes.cpp Bytes.h
	g++ -c Bytes.cpp -O3
edt-test: edt.o Bytes.o
	g++ edt.o Bytes.o -o edt-test -O3 -lcrypto