OBJS=lm.o
TESTOBJS=tests.o


all: lm.a tests liblm.so

clean:
	rm -f $(OBJS) $(TESTOBJS) lm.a liblm.so tests

COPTS=-O0 -g -fPIC
#-fprofile-arcs -ftest-coverage
#COPTS=-O3 -Wall
CPPOPTS=-O0 -g -Wall -ggdb3 -fPIC
#-fprofile-arcs -ftest-coverage




.PHONY: calibrate/calibrate.a

%.o: %.c
	gcc -c $^ -o $@ -I. -Iapi $(COPTS)

%.o: %.cc
	g++ -c $^ -o $@ $(CPPOPTS)

tests: $(TESTOBJS) lm.a
	g++ $^ -lrt -ldl -pthread -o $@ $(CPPOPTS) lm.a

liblm.so: $(OBJS)
	g++ -shared $(OBJS) -o liblm.so

lm.a: $(OBJS)
	ar -r lm.a $(OBJS)

