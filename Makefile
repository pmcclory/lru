all: liblru.so test

DEBUG := 1

CFLAGS:= -Wall -Werror

ifeq ($(DEBUG), 1)
CFLAGS += -ggdb -O0
endif

liblru.so: lru.c lru.c
	$(CC) -fPIC -c $(CFLAGS) -o $@ $^

test: test.c liblru.so
	$(CC) $(CFLAGS) -o $@ -llru -L. test.c

clean:
	rm -rf liblru.so test
