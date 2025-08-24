TARGET = kxo
kxo-objs = main.o game.o xoroshiro.o mcts.o negamax.o zobrist.o
obj-m := $(TARGET).o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied
all: kmod xo-user

kmod: $(GIT_HOOKS) main.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

xo-user: xo-user.c
	$(CC) $(ccflags-y) -g -o $@ $<

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
