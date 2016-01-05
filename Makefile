OBJS=src/lm.o
TESTOBJS=tests/tests.o

CIVETWEB_DIR=../civetweb
WEBOBJS=view/web.o view/file.o view/folder.o

CIVETWEB_LIB = $(CIVETWEB_DIR)/libcivetweb.a

all: lm.a tests.exe liblm.so web.exe

clean:
	rm -f $(OBJS) $(WEBOBJS) $(TESTOBJS) lm.a liblm.so tests.exe web.exe

COPTS=-O0 -g -fPIC
#-fprofile-arcs -ftest-coverage
#COPTS=-O3 -Wall
CPPOPTS=-O0 -g -Wall -ggdb3 -fPIC -I./src -I$(CIVETWEB_DIR)/include
#-fprofile-arcs -ftest-coverage




.PHONY: 

%.o: %.c
	gcc -c $^ -o $@ -I. -Iapi $(COPTS)

%.o: %.cc
	g++ -c $^ -o $@ $(CPPOPTS)

view/folder.o: view/folder.png
	cd view; ld -r -b binary -o folder.o folder.png
	
view/file.o: view/file.png
	cd view; ld -r -b binary -o file.o file.png

tests.exe: $(TESTOBJS) lm.a
	g++ $^ -lrt -ldl -pthread -o $@ $(CPPOPTS) -I./src lm.a

liblm.so: $(OBJS)
	g++ -shared $(OBJS) -o liblm.so

lm.a: $(OBJS)
	ar -r lm.a $(OBJS)

web.exe: $(WEBOBJS) lm.a
	g++ $^ -o $@ $(CPPOPTS) lm.a $(CIVETWEB_LIB) -lrt -ldl -pthread
