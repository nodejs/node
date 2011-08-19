LDFLAGS=-L../.. -lcrypto
CFLAGS=-I../../include -Wall -Werror -g

all: jpakedemo

jpakedemo: jpakedemo.o
	$(CC) -g -o jpakedemo jpakedemo.o $(LDFLAGS)
