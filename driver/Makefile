CFLAGS = -Wall -Wextra -O2 -framework IOKit -framework CoreFoundation

all: ada8xrctl

ada8xrctl: ada8xrctl.c fwio.c bebob.h fwio.h
	clang $(CFLAGS) -o ada8xrctl ada8xrctl.c fwio.c

clean:
	rm -f ada8xrctl _api_test

.PHONY: clean
