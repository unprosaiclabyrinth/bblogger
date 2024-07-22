CC     = gcc
CFLAGS = -Wall -Wextra -shared -fPIC

all: bblogger.so

bblogger.so: bblogger.c clean
	$(CC) $(CFLAGS) -o $@ $< -I/path/to/dynamorio/build/include -L/path/to/dynamorio/build/lib64 -DLINUX -DX86_64

clean:
	@rm -f bblogger.so

.PHONY: clean