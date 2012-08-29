# weeplugins - Plugins for WeeChat

weeplugins are plugins for the IRC client [WeeChat](http://www.weechat.org/ "WeeChat, the extensible chat client.").

## weenick: Provides support for common NickServ operations

weenick allows you to define global and/or server-specific options to get automatically identified with services (NickServ).

Upon connection, if the nick is already in use the GHOST command will be sent, and once it has been killed you'll change nick. The IDENTIFY command is then sent.

You can also have command(s) executed once identified.

 
## weereact: Triggers commands in reaction to messages

weereact allows you to have commands be executed by reacting on messages you receive (or send). They can be filtered by server, channel, user, and content (through perl-compatible regex).

It also introduces a new command - `/tobuffer` - to send text (or commands) to a specific buffer.
