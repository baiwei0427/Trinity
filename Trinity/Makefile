obj-m+=trinity.o
EXTRA_CFLAGS +=-DTRINITY

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	gcc user.c control.h -o user.o
	mknod /dev/trinity c 100 1
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm /dev/trinity

	
