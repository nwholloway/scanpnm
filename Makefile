# Configuration (change here, or in scanpnm.h)
#  -DDEFDPI=200
#  -DDEFFMT="ppm"
#  -DTMPDIR=\"/var/tmp\"
#  -DTMPNAM=\"/scanpnmXXXXXX\"
#  -DDEVICE=\"/dev/scanner\"

CUSTOM  = -DDEVICE=\"/dev/ttyb\"

CC      = gcc
CFLAGS  = -O2 $(CUSTOM)
LDFLAGS = -s

BINDIR  = /dcs/share/bin
MANDIR  = /dcs/share/man
MANSEC  = 1

scanpnm: scanpnm.o jx100.o util.o
	$(CC) $(LDFLAGS) -o scanpnm scanpnm.o jx100.o

install: scanpnm
	install -c scanpnm $(BINDIR)
#	install -c scanpnm.man $(MANDIR)/man$(MANSEC)/scanpnm.$(MANSEC)

jx100.o: jx100.c jx100.h

scanpnm.o: scanpnm.c scanpnm.h jx100.h
util.o: util.c util.h

clean:
	rm -f *.o core
clobber: clean
	rm -f scanpnm
