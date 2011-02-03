CC=cc
CFLAGS=-c -Wall
LDFLAGS=
BINARIES=fnvum fnctl

all: $(BINARIES)

clean:
	rm -rf *.o

fnvum: fnvum.c dft.c libfn.c
	$(CC) $(LDFLAGS) dft.o fnvum.o libfn.o -o bin/fnvum `sdl-config --cflags --libs` -l pulse-simple
	
fnvum_fftw: fnvum_fftw.c libfn.c
	$(CC) $(LDFLAGS) fnvum_fftw.o libfn.o -o bin/fnvum_fftw `sdl-config --cflags --libs` -l pulse-simple -l fftw3 -g
	
fnpom: fnpom.c libfn.c
	$(CC) $(LDFLAGS) fnpom.o libfn.o -o bin/fnpom -l json
	
fnctl: libfn.c fnctl.c
	$(CC) $(LDFLAGS) fnctl.o libfn.o -o bin/fnctl

fnvum.c:
	$(CC) $(CFLAGS) src/fnvum.c -o fnvum.o
	
fnpom.c:
	$(CC) $(CFLAGS) src/fnpom.c -o fnpom.o -g
	
fnvum_fftw.c:
	$(CC) $(CFLAGS) src/fnvum_fftw.c -o fnvum_fftw.o -g

fnctl.c:
	$(CC) $(CFLAGS) src/fnctl.c -o fnctl.o

libfn.c:
	$(CC) $(CFLAGS) lib/libfn.c -o libfn.o

dft.c:
	$(CC) $(CFLAGS) lib/dft.c -o dft.o -lm
