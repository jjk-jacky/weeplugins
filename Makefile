
.PHONY = all install uninstall clean

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
	-Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
	-Wuninitialized -Wconversion -Wstrict-prototypes
CFLAGS := -g -std=c99 -fPIC $(WARNINGS)


all: weenick.so weereact.so

weereact.o: weereact.c
	$(CC) $(CFLAGS) -O2 -c `pkg-config --cflags glib-2.0` -o weereact.o weereact.c

%.o: %.c
	$(CC) $(CFLAGS) -O2 -c -o $@ $<

weenick.so: weenick.o
	$(CC) $(CFLAGS) -shared -o weenick.so weenick.o

weereact.so: weereact.o
	$(CC) $(CFLAGS) -shared `pkg-config --libs glib-2.0` -o weereact.so weereact.o

clean:
	rm -f *.o *.so

install:
	$(INSTALL) -s -m755 weenick.so /usr/lib/weechat/plugins/weenick.so
	$(INSTALL) -s -m755 weereact.so /usr/lib/weechat/plugins/weereact.so

uninstall:
	rm /usr/lib/weechat/plugins/weenick.so
	rm /usr/lib/weechat/plugins/weereact.so
