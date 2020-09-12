CC = /usr/hitech/bin/picl
CFILES = aprstracker.c
RELEASE_CFLAGS = -asmlist -O
DEBUG_CFLAGS = -asmlist -O -DDEBUG
VERSION = 0.12

ifeq "$(VERSION)" "DEBUG"
	CFLAGS = $(DEBUG_CFLAGS)
else
	CFLAGS = $(RELEASE_CFLAGS)
endif

all: programmermenu tracker

tracker:
	$(CC) $(CFILES) $(CFLAGS) -16F628 | grep -v HI-TECH | tee aprstracker-$(VERSION)-16f628.meminfo
	  todos <aprstracker.hex >aprstracker-$(VERSION)-16f628.hex
	$(CC) $(CFILES) $(CFLAGS) -16F636 | grep -v HI-TECH | tee aprstracker-$(VERSION)-16f636.meminfo
	  todos <aprstracker.hex >aprstracker-$(VERSION)-16f636.hex
	$(CC) $(CFILES) $(CFLAGS) -16F648 | grep -v HI-TECH | tee aprstracker-$(VERSION)-16f648.meminfo
	  todos <aprstracker.hex >aprstracker-$(VERSION)-16f648.hex
#	$(CC) $(CFILES) $(CFLAGS) -16F88 | grep -v HI-TECH | tee aprstracker-$(VERSION)-16f88.meminfo
#	  todos <aprstracker.hex >aprstracker-$(VERSION)-16f88.hex
	rm aprstracker.hex

programmermenu:
	diet gcc -Wall -Os atprogrammenu.c -o atprogrammenu
#	gcc -Wall -Os atprogrammenu.c -o atprogrammenu

distclean:
	rm -f *.obj *.dep *.lst *.cod *.cof *.sym *.rlf *.bak

clean: distclean
	rm -f *.hex *.HEX atprogrammenu
