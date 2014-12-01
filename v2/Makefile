
TARGETS := person

PERSON_OBJS := person.o

OBJS := $(PERSON_OBJS)

CC := gcc

CFLAGS += -D_REENTRANT -D_LIBC_REENTRANT -D_THREAD_SAFE
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Wshadow
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -g -O2

%.o: %.c
	$(CC) -o $*.o $< -c $(CFLAGS)

.PHONY: all clean

all: $(TARGETS)

person.o : person.h

clean:
	-rm -f $(TARGETS) $(OBJS) *~ *.bak core*

person: $(PERSON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

