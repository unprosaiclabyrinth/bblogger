CC     = gcc
CFLAGS = -Wall -Wextra -shared -fPIC

all: bblogger.so

bblogger.so: bblogger.c clean
	$(CC) $(CFLAGS) -o $@ $< -I/users/hdongr2/dynamorio/build/include -L/users/hdongr2/dynamorio/build/lib64 -DLINUX -DX86_64 -DVERBOSE

clean:
	@rm -f bblogger.so

.PHONY: clean