CC=gcc
CFLAGS=-c -Wall -I. -fpic -g -fbounds-check
CFLAGS=-c -Wall -I. -fpic -g -fbounds-check -Werror

OBJS=tester.o reference.o student.o

%.o:	%.c %.h
	$(CC) $(CFLAGS) $< -o $@

tester:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f student.o tester.o tester