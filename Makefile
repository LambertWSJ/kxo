TARGET = kxo
kxo-objs = main.o game.o xoroshiro.o mcts.o negamax.o zobrist.o
obj-m := $(TARGET).o
OBJS:=

ccflags-y := -std=gnu99 -Wno-declaration-after-statement
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied

LDFLAGS:=
CFLAGS:=
CFLAGS+=-g

OBJS:=
OBJS+=xo-user.o
OBJS+=tui.o

all: kmod xo-user

kmod: $(GIT_HOOKS) main.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

xo-user: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %c
	$(CC) $< $(CFLAGS) -c -o $@


logo:
	cat logo.txt | lolcat -f > logof.txt && cat ./logof.txt

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

insmod: kmod
	sudo insmod $(TARGET).ko

rmmod:
	sudo rmmod $(TARGET).ko

format:
	clang-format *.[ch] -i

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) xo-user
