
SOURCES = main
CPPFLAGS = -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -O2 #-g -pg
LINKFLAGS = -O2 #-g -pg

CPP = g++

all: includecull.exe

include $(SOURCES:=.d)

includecull.exe: $(SOURCES:=.o) makefile
	$(CPP) -o $@ $(SOURCES:=.o) $(LINKFLAGS) 

asm: $(SOURCES:=.S) makefile

clean:
	rm -rf *.o *.exe *.d *.S

run: includecull.exe
	includecull.exe

%.o: %.cpp makefile
	$(CPP) $(CPPFLAGS) -c -o $@ $<

%.S: %.cpp makefile
	$(CPP) $(CPPFLAGS) -c -g -Wa,-a,-ad $< > $@

%.d: %.cpp makefile
	bash -ec '$(CPP) $(CPPFLAGS) -MM $< | sed "s!$*.o!$*.o $@!g" > $@'
