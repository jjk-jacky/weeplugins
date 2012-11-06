/**
 * weeplugins - Copyright (C) 2012 Olivier Brunel
 *
 * weenick.c
 * Copyright (C) 2012 Olivier Brunel <i.am.jack.mail@gmail.com>
 * 
 * This file is part of weeplugins.
 *
 * weeplugins is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * weeplugins is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * weeplugins. If not, see http://www.gnu.org/licenses/
 */

/* C */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* WeeChat */
#include <weechat/weechat-plugin.h>

#define _UNUSED_                            __attribute__ ((unused)) 
#define BUF_SIZE                            255

/* prefixes for our options: default, or per-server */
#define OPT_PREFIX_DEFAULT                  "server_default"
#define OPT_PREFIX_SERVER                   "server."

/* options */
#define OPT_NICK                            ".nick"
#define OPT_NICK_DESC                       "registered nickname"

#define OPT_PASSWORD                        ".password"
#define OPT_PASSWORD_DESC                   "password to identify/kill ghosts"

#define OPT_COMMAND                         ".command"
#define OPT_COMMAND_DESC                    "command(s) to get processed upon identification"

#define OPT_NICKSERV_NICK                   ".nickserv_nick"
#define OPT_NICKSERV_NICK_DESC              "nickname to send messages to"
#define OPT_NICKSERV_NICK_DEFAULT           "NickServ"

#define OPT_NICKSERV_MSG_REGISTERED         ".nickserv_registered"
#define OPT_NICKSERV_MSG_REGISTERED_DESC    "string to identify notice that nick is registered"
#define OPT_NICKSERV_MSG_REGISTERED_DEFAULT "nickname is registered"

#define OPT_NICKSERV_MSG_GHOST              ".nickserv_ghost_killed"
#define OPT_NICKSERV_MSG_GHOST_DESC         "string to identify notice that ghost was killed"
#define OPT_NICKSERV_MSG_GHOST_DEFAULT      "ghost with your nick has been killed"

#define OPT_NICKSERV_MSG_IDENTIFIED         ".nickserv_identified"
#define OPT_NICKSERV_MSG_IDENTIFIED_DESC    "string to identify notice that nick was identified"
#define OPT_NICKSERV_MSG_IDENTIFIED_DEFAULT "password accepted"

#define OPT_NICKSERV_MSG_FAILED             ".nickserv_failed"
#define OPT_NICKSERV_MSG_FAILED_DESC        "string to identify notice that password is wrong"
#define OPT_NICKSERV_MSG_FAILED_DEFAULT     "access denied"



WEECHAT_PLUGIN_NAME ("weenick")
WEECHAT_PLUGIN_AUTHOR ("jjacky")
WEECHAT_PLUGIN_DESCRIPTION ("Provides support for common NickServ operations")
WEECHAT_PLUGIN_VERSION ("0.1")
WEECHAT_PLUGIN_LICENSE ("GPL3")

static struct t_weechat_plugin *weechat_plugin = NULL;

static const char *
get_option_for_server (const char *server, const char *option)
{
    char buf[BUF_SIZE];
    
    snprintf (buf, BUF_SIZE, "%s%s%s", OPT_PREFIX_SERVER, server, option);
    if (!weechat_config_is_set_plugin (buf))
    {
        /* no option for this server, fallback on default */
        snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, option);
        if (!weechat_config_is_set_plugin (buf))
        {
            return NULL;
        }
    }
    return weechat_config_get_plugin (buf);
}

static int
get_nickserv_msg_for_server (const char *server, const char *option, char *buf)
{
    const char *value;
    
    if (!(value = get_option_for_server (server, option)))
    {
        return 0;
    }
    
    snprintf (buf, BUF_SIZE, "*%s*", value);
    return 1;
}

static struct t_gui_buffer *
get_buffer (const char *signal, char *server_name)
{
    struct t_gui_buffer *buffer;
    char  server[BUF_SIZE];
    char *s;
    size_t len;
    
    for (s = (char *) signal, len = 0; *s != ','; ++s, ++len)
        ;
    len += 8; /* 8 = strlen ("server.") + 1 for NULL */
    /* to be safe, but it should always be under 255 chars */
    if (len >= BUF_SIZE)
    {
        return NULL;
    }
    snprintf (server, len, "server.%s", signal);
    
    /* copy server name if asked for */
    if (server_name)
    {
        snprintf (server_name, BUF_SIZE, "%s", server + 7);
    }
    
    /* we'll return the found buffer, or NULL if not found */
    buffer = weechat_buffer_search ("irc", server);
    return buffer;
}

static int
in_use_cb (void *data _UNUSED_, const char *signal, const char *type_data _UNUSED_,
           void *signal_data)
{
    /*  signal          server,irc_in2_433
     *  signal_data     :nick!ident@host.name 433 yournick wantnick :message
     */
    
    struct t_gui_buffer *buffer;
    char  server[BUF_SIZE];
    char  buf[BUF_SIZE];
    const char *want_nick;
    const char *nickserv;
    const char *password;
    char *s;
    size_t l;
    
    /* get the server's name & its buffer */
    if (!(buffer = get_buffer (signal, server)))
    {
        return WEECHAT_RC_ERROR;
    }
    
    /* get the nick we want */
    if (!(want_nick = get_option_for_server (server, OPT_NICK)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* get our nickserv password */
    if (!(password = get_option_for_server (server, OPT_PASSWORD)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* get nickserv's nick */
    if (!(nickserv = get_option_for_server (server, OPT_NICKSERV_NICK)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* check the new nick (taken) is our want_nick */
    if (!(s = strstr (signal_data, " 433 ")))
    {
        return WEECHAT_RC_ERROR;
    }
    if (!(s = strchr (s + 5, ' '))) /* 5 = strlen (" 433 ") */
    {
        return WEECHAT_RC_ERROR;
    }
    l = strlen (want_nick);
    ++s;
    if (strncmp (s, want_nick, l) != 0 || s[l] != ' ')
    {
        /* the nick we tried to get isn't our want_nick, done */
        return WEECHAT_RC_OK;
    }
    /* let's kill that ghost */
    snprintf (buf, BUF_SIZE, "/msg %s GHOST %s %s", nickserv, want_nick, password);
    weechat_command (buffer, buf);
    return WEECHAT_RC_OK;
}

static int
notice_cb (void *data _UNUSED_, const char *signal, const char *type_data _UNUSED_,
           void *_signal_data)
{
    /*  signal          server,irc_in2_notice
     *  signal_data     :nick!ident@host.name NOTICE yournick :message
     */
    
    char *signal_data = _signal_data;
    struct t_gui_buffer *buffer;
    char  server[BUF_SIZE];
    char  buf[BUF_SIZE];
    char *msg;
    char *cur_nick;
    const char *want_nick;
    const char *nickserv;
    const char *password;
    char *s;
    size_t l;
    
    /* get the server's name & its buffer */
    if (!(buffer = get_buffer (signal, server)))
    {
        return WEECHAT_RC_ERROR;
    }
    
    /* get the nick we want */
    if (!(want_nick = get_option_for_server (server, OPT_NICK)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* get our nickserv password */
    if (!(password = get_option_for_server (server, OPT_PASSWORD)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* get nickserv's nick */
    if (!(nickserv = get_option_for_server (server, OPT_NICKSERV_NICK)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* make sure the sender is NickServ */
    if (!(s = strchr (signal_data, '!')))
    {
        return WEECHAT_RC_ERROR;
    }
    l = strlen (nickserv);
    ++signal_data;
    if (weechat_strncasecmp (signal_data, nickserv, (int) l) != 0
        || ((char *) signal_data)[l] != '!')
    {
        /* ignore every other notice */
        return WEECHAT_RC_OK;
    }
    
    /* determine whether we have our desired nick or not */
    if (!(s = strstr (signal_data, " NOTICE ")))
    {
        return WEECHAT_RC_ERROR;
    }
    cur_nick = s + 8; /* 8 = strlen (" NOTICE ") */
    msg = strchr (s, ':');
    if (!(s = strchr (cur_nick, ' ')) || s[1] != ':')
    {
        return WEECHAT_RC_ERROR;
    }
    l = strlen (want_nick);
    if (weechat_strncasecmp (cur_nick, want_nick, (int) l) != 0
        || cur_nick[l] != ' ')
    {
        /* not our nick; look if a ghost was just killed */
        if (get_nickserv_msg_for_server (server, OPT_NICKSERV_MSG_GHOST, buf))
        {
            if (weechat_string_match (msg, buf, 0))
            {
                /* ghost was killed, grab our nick back */
                snprintf (buf, BUF_SIZE, "/nick %s", want_nick);
                weechat_command (buffer, buf);
                return WEECHAT_RC_OK;
            }
        }
        return WEECHAT_RC_OK;
    }
    
    /* we have our desired nick; do we need to identify? */
    if (get_nickserv_msg_for_server (server, OPT_NICKSERV_MSG_REGISTERED, buf))
    {
        if (weechat_string_match (msg, buf, 0))
        {
            /* identify ourself */
            snprintf (buf, BUF_SIZE, "/msg %s IDENTIFY %s",
                        nickserv, password);
            weechat_command (buffer, buf);
            return WEECHAT_RC_OK;
        }
    }
    /* or did we just get identified? */
    if (get_nickserv_msg_for_server (server, OPT_NICKSERV_MSG_IDENTIFIED, buf))
    {
        if (weechat_string_match (msg, buf, 0))
        {
            /* trigger command(s) */
            if ((s = (char *) get_option_for_server (server, OPT_COMMAND)))
            {
                char **commands = weechat_string_split_command (s, ';');
                int i;
                for (i = 0; commands[i]; ++i)
                {
                    if (weechat_string_is_command_char (commands[i]))
                    {
                        weechat_command (buffer, commands[i]);
                    }
                }
                weechat_string_free_split_command (commands);
            }
            return WEECHAT_RC_OK;
        }
    }
    /* password refused, from an attempt to identify or kill ghost */
    if (get_nickserv_msg_for_server (server, OPT_NICKSERV_MSG_FAILED, buf))
    {
        if (weechat_string_match (msg, buf, 0))
        {
            weechat_printf (NULL, "%sPassword failed", weechat_prefix ("error"));
            return WEECHAT_RC_OK;
        }
    }
    return WEECHAT_RC_OK;
}

static int
welcome_cb (void *data _UNUSED_, const char *signal, const char *type_data _UNUSED_,
            void *signal_data)
{
    /*  signal          server,irc_in2_001
     *  signal_data     :server.addr 001 NICK :Welcome message
     */
    
    struct t_gui_buffer *buffer;
    char  server[BUF_SIZE];
    char *cur_nick;
    int   cur_nick_len;
    const char *want_nick;
    char *s;
    
    /* get the server's name & its buffer */
    if (!(buffer = get_buffer (signal, server)))
    {
        return WEECHAT_RC_ERROR;
    }
    
    /* get the nick we want */
    if (!(want_nick = get_option_for_server (server, OPT_NICK)))
    {
        /* not set, we're done */
        return WEECHAT_RC_OK;
    }
    
    /* get the nick we have */
    if (!(s = strchr (signal_data, ' ')) || strncmp (++s, "001 ", 4) != 0)
    {
        return WEECHAT_RC_ERROR;
    }
    cur_nick = s;
    if (!(s = strchr (cur_nick, ' ')))
    {
        return WEECHAT_RC_ERROR;
    }
    cur_nick_len = (int) (s - cur_nick);
    
    if (strncmp (want_nick, cur_nick, (size_t) cur_nick_len) == 0
        && want_nick[cur_nick_len] == '\0')
    {
        /* match, we're good */
        return WEECHAT_RC_OK;
    }
    
    /* let's try to change our nick then */
    char cmd[BUF_SIZE];
    snprintf (cmd, BUF_SIZE, "/nick %s", want_nick);
    weechat_command (buffer, cmd);
    
    return WEECHAT_RC_OK;
}

int
weechat_plugin_init (struct t_weechat_plugin *plugin,
                     int argc _UNUSED_,
                     char *argv[] _UNUSED_)
{
    char buf[BUF_SIZE];
    
    weechat_plugin = plugin;

    /* set descriptions for our options, and default value when applicable */
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICK);
    weechat_config_set_desc_plugin (buf, OPT_NICK_DESC);
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_PASSWORD);
    weechat_config_set_desc_plugin (buf, OPT_PASSWORD_DESC);
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_COMMAND);
    weechat_config_set_desc_plugin (buf, OPT_COMMAND_DESC);
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICKSERV_NICK);
    weechat_config_set_desc_plugin (buf, OPT_NICKSERV_NICK_DESC);
    if (!weechat_config_is_set_plugin (buf))
    {
        weechat_config_set_plugin (buf, OPT_NICKSERV_NICK_DEFAULT);
    }
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICKSERV_MSG_REGISTERED);
    weechat_config_set_desc_plugin (buf, OPT_NICKSERV_MSG_REGISTERED_DESC);
    if (!weechat_config_is_set_plugin (buf))
    {
        weechat_config_set_plugin (buf, OPT_NICKSERV_MSG_REGISTERED_DEFAULT);
    }
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICKSERV_MSG_GHOST);
    weechat_config_set_desc_plugin (buf, OPT_NICKSERV_MSG_GHOST_DESC);
    if (!weechat_config_is_set_plugin (buf))
    {
        weechat_config_set_plugin (buf, OPT_NICKSERV_MSG_GHOST_DEFAULT);
    }
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICKSERV_MSG_IDENTIFIED);
    weechat_config_set_desc_plugin (buf, OPT_NICKSERV_MSG_IDENTIFIED_DESC);
    if (!weechat_config_is_set_plugin (buf))
    {
        weechat_config_set_plugin (buf, OPT_NICKSERV_MSG_IDENTIFIED_DEFAULT);
    }
    
    snprintf (buf, BUF_SIZE, "%s%s", OPT_PREFIX_DEFAULT, OPT_NICKSERV_MSG_FAILED);
    weechat_config_set_desc_plugin (buf, OPT_NICKSERV_MSG_FAILED_DESC);
    if (!weechat_config_is_set_plugin (buf))
    {
        weechat_config_set_plugin (buf, OPT_NICKSERV_MSG_FAILED_DEFAULT);
    }
    
    
    /* hook to message RPL_WELCOME (001) */
    weechat_hook_signal ("*,irc_in2_001", &welcome_cb, NULL);
    
    /* hook to message ERR_NICKNAMEINUSE (433) */
    weechat_hook_signal ("*,irc_in2_433", &in_use_cb, NULL);
    
    /* hook to message NOTICE */
    weechat_hook_signal ("*,irc_in2_notice", &notice_cb, NULL);
    
    return WEECHAT_RC_OK;
}

int
weechat_plugin_end (struct t_weechat_plugin *plugin _UNUSED_)
{
    return WEECHAT_RC_OK;
}
