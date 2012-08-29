
.PHONY = all install uninstall clean

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
	-Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
	-Wuninitialized -Wconversion -Wstrict-prototypes
CFLAGS := -g -std=c99 -fPIC $(WARNINGS)

CC = cc
INSTALL = install -s

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
	$(INSTALL) -Dm755 weenick.so  $(DESTDIR)/usr/lib/weechat/plugins/weenick.so
	$(INSTALL) -Dm755 weereact.so $(DESTDIR)/usr/lib/weechat/plugins/weereact.so

uninstall:
	rm $(DESTDIR)/usr/lib/weechat/plugins/weenick.so
	rm $(DESTDIR)/usr/lib/weechat/plugins/weereact.so
