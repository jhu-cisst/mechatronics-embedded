

SRCS = fpgav3_emio.cpp fpgav3_emio_gpiod.cpp fpgav3_emio_mmap.cpp fpgav3_qspi.cpp fpgav3_lib.cpp
OBJS = fpgav3_emio.o fpgav3_emio_gpiod.o fpgav3_emio_mmap.o fpgav3_qspi.o fpgav3_lib.o

VERSION = 1.1

all: libfpgav3.so

libfpgav3.so.$(VERSION): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -shared -Wl,-soname,libfpgav3.so.$(VERSION) -o libfpgav3.so.$(VERSION) $(LDLIBS)

libfpgav3.so: libfpgav3.so.$(VERSION)
	rm -f libfpgav3.so
	ln -s libfpgav3.so.$(VERSION) libfpgav3.so

%.o: %.c
	$(CC) $(CFLAGS) -c -fPIC $(SRCS)

clean:
	rm -f *.o *.so*
