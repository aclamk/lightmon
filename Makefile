OBJS=src/lm.o
TESTOBJS=tests/tests.o


all: lm.a tests.exe liblm.so

clean:
	rm -f $(OBJS) $(TESTOBJS) lm.a liblm.so tests.exe

COPTS=-O0 -g -fPIC
#-fprofile-arcs -ftest-coverage
#COPTS=-O3 -Wall
CPPOPTS=-O0 -g -Wall -ggdb3 -fPIC -I./src
#-fprofile-arcs -ftest-coverage




.PHONY: 

%.o: %.c
	gcc -c $^ -o $@ -I. -Iapi $(COPTS)

%.o: %.cc
	g++ -c $^ -o $@ $(CPPOPTS)

tests.exe: $(TESTOBJS) lm.a
	g++ $^ -lrt -ldl -pthread -o $@ $(CPPOPTS) -I./src lm.a

liblm.so: $(OBJS)
	g++ -shared $(OBJS) -o liblm.so

lm.a: $(OBJS)
	ar -r lm.a $(OBJS)

