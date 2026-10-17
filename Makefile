# Root Makefile — build all user-space tools.
#
# Command Line Tools alone are sufficient: xcode-select --install
# (or DEVELOPER_DIR=/Library/Developer/CommandLineTools make ...)

CFLAGS  = -Wall -Wextra -O2
FWLIBS  = -framework IOKit -framework CoreFoundation

.PHONY: all clean tools driver

all: tools driver

tools: tools/fwprobe/fwprobe tools/setout

driver: driver/ada8xrctl

tools/fwprobe/fwprobe: tools/fwprobe/fwprobe.c
	$(CC) $(CFLAGS) -o $@ $< $(FWLIBS)

tools/setout: tools/setout.c
	$(CC) $(CFLAGS) -o $@ $< -framework CoreAudio -framework CoreFoundation

driver/ada8xrctl: driver/ada8xrctl.c driver/fwio.c driver/bebob.h driver/fwio.h
	$(CC) $(CFLAGS) -o $@ driver/ada8xrctl.c driver/fwio.c $(FWLIBS)

clean:
	rm -f tools/fwprobe/fwprobe tools/setout driver/ada8xrctl
