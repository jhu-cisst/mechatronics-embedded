

SRCS = fpgav3_emio.c fpgav3_qspi.c
OBJS = fpgav3_emio.o fpgav3_qspi.o

VERSION = 1.0

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
