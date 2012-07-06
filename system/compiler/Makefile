all: tokens.lex parser.y gate.o optimizer deadgates binary identitygates fairplay
	flex tokens.lex
	bison -d parser.y
	flex tokens.lex
	gcc -o parser parser.tab.c lex.yy.c gate.o -lm -O3
gate.o: gate.c
	gcc -c gate.c -Wall -Werror
debug:
	gcc -o parser parser.tab.c lex.yy.c gate.c -lm -g -DDEBUG_ENABLE
	g++ deadgates.cpp util.o gate.o -o deadgates -g
	g++ binary.cpp util.o -o binary -g
optimizer: optimize.o gate.o
	g++ optimize.o gate.o -o optimize -O3
optimize.o: optimize.cpp
	g++ -c optimize.cpp -Wall -O3
deadgates: deadgates.o util.o gate.o
	g++ deadgates.o util.o gate.o -o deadgates -O3
util.o: util.h util.cpp
	g++ util.cpp -c -Wall -O3
deadgates.o: deadgates.cpp
	g++ deadgates.cpp -c -Wall -O3
binary.o: binary.cpp
	g++ binary.cpp -c -Wall -Werror -O3
binary: binary.o util.o
	g++ binary.o util.o -o binary -O3
identitygates.o: identitygates.cpp
	g++ identitygates.cpp -c -Wall -Werror -O3
identitygates: identitygates.o util.o
	g++ identitygates.o util.o -o identitygates -O3
fairplay.o: fairplay.cpp
	g++ fairplay.cpp -c -Wall -Werror -O3
fairplay: fairplay.o util.o
	g++ fairplay.o util.o -o fairplay -O3