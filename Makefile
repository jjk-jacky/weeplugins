
.PHONY = all install uninstall clean

WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wmissing-prototypes -Wmissing-declarations \
	-Wredundant-decls -Wnested-externs -Winline -Wno-long-long \
	-Wuninitialized -Wconversion -Wstrict-prototypes
CFLAGS := -g -std=c99 -fPIC $(WARNINGS)

CC = cc
INSTALL = install

all: weenick.so weereact.so

weereact.o: weereact.c
	$(CC) $(CFLAGS) -O2 -c `pkg-config --cflags glib-2.0` -o weereact.o weereact.c

%.o: %.c
	$(CC) $(CFLAGS) -O2 -c -o $@ $<

weenick.so: weenick.o
	$(CC) $(CFLAGS) -shared -o weenick.so weenick.o

weereact.so: weereact.o
	$(CC) $(CFLAGS) -shared -o weereact.so weereact.o `pkg-config --libs glib-2.0`

clean:
	rm -f *.o *.so

install:
	$(INSTALL) -s -Dm755 weenick.so  $(DESTDIR)/usr/lib/weechat/plugins/weenick.so
	$(INSTALL) -s -Dm755 weereact.so $(DESTDIR)/usr/lib/weechat/plugins/weereact.so
	$(INSTALL) -Dm644 AUTHORS        $(DESTDIR)/usr/share/doc/weechat/weeplugins/AUTHORS
	$(INSTALL) -Dm644 COPYING        $(DESTDIR)/usr/share/doc/weechat/weeplugins/COPYING
	$(INSTALL) -Dm644 README.md      $(DESTDIR)/usr/share/doc/weechat/weeplugins/README.md
	$(INSTALL) -Dm644 weereact.conf  $(DESTDIR)/usr/share/doc/weechat/weeplugins/weereact.conf

uninstall:
	rm $(DESTDIR)/usr/lib/weechat/plugins/weenick.so
	rm $(DESTDIR)/usr/lib/weechat/plugins/weereact.so
	rm -rf $(DESTDIR)/usr/share/doc/weechat/weeplugins
	rmdir --ignore-fail-on-non-empty $(DESTDIR)/usr/share/doc/weechat
