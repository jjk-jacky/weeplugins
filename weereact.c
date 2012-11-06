/**
 * weeplugins - Copyright (C) 2012 Olivier Brunel
 *
 * weereact.c
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

/* for strdup() in string.h */
#define _BSD_SOURCE

/* C */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* GLib (for PCRE) */
#include <glib.h>

/* WeeChat */
#include <weechat/weechat-plugin.h>

#define _UNUSED_                            __attribute__ ((unused)) 
#define BUF_SIZE                            255



WEECHAT_PLUGIN_NAME ("weereact")
WEECHAT_PLUGIN_AUTHOR ("jjacky")
WEECHAT_PLUGIN_DESCRIPTION ("Triggers commands in reaction to messages")
WEECHAT_PLUGIN_VERSION ("0.1")
WEECHAT_PLUGIN_LICENSE ("GPL3")

static struct t_weechat_plugin *weechat_plugin = NULL;
static GRegex *regex_strip_color;

typedef struct _react_t {
    char            *on;
    char            *to;
    char            *by;
    char            *is;
    GRegex          *regex;
    char           **cmd;
    struct _react_t *next;
} react_t;

static char *
trim (char *str)
{
    char *s, *e;
    
    if (!str || *str == '\0')
    {
        return str;
    }
    
    for ( ; *str == ' ' || *str == '\t'; ++str)
        ;
    s = str;
    for (e = str; *str; ++str)
    {
        if (*str != ' ' && *str != '\t')
        {
            e = str;
        }
    }
    *++e = '\0';
    
    return s;
}

static react_t *react = NULL;

static inline void
add_item (react_t *new, int nb_used, char **cmd)
{
    react_t *r;
    int i;
    
    r = malloc (sizeof (*r));
    memcpy (r, new, sizeof (*r));
    r->cmd = calloc (sizeof (r->cmd), (size_t) nb_used + 1);
    for (i = 0; i < nb_used; ++i)
    {
        r->cmd[i] = cmd[i];
    }
    if (react)
    {
        react_t *n;
        for (n = react; n->next; n = n->next)
            ;
        n->next = r;
    }
    else
    {
        react = r;
    }
}

static int
load_config (void)
{
    char  file[BUF_SIZE];
    char *data;
    
    char *s, *e;
    size_t l;
    int linenum = 0;
    
    int is_item = 0;
    react_t item;
    int nb_alloc = 0;
    int nb_used = 0;
    char **cmd = NULL;
    
    snprintf (file, BUF_SIZE, "%s/weereact.conf",
              weechat_info_get ("weechat_dir", NULL));
    if (!(data = weechat_file_get_content (file)))
    {
        weechat_printf (NULL, "%sCould not load config from %s",
                        weechat_prefix ("error"), file);
        return 0;
    }
    
    s = data;
    while ((e = strchr (s, '\n')) || (*s))
    {
        char *val;

        ++linenum;
        if (e)
        {
            *e = '\0';
        }
        s = trim (s);
        if (*s == '#' || *s == '\0')
        {
            goto next;
        }
        
        l = strlen (s);
        if (*s == '[' && s[l - 1] == ']')
        {
            if (is_item)
            {
                add_item (&item, nb_used, cmd);
            }
            
            is_item = 1;
            memset (&item, '\0', sizeof (react_t));
            nb_used = 0;
            goto next;
        }
        else if (!is_item)
        {
            weechat_printf (NULL, "%sError in %s, line %d: not in section",
                            weechat_prefix ("error"), file, linenum);
            goto next;
        }
        
        if (!(val = strchr (s, '=')))
        {
            goto next;
        }
        *val++ = '\0';
        
        if (strcmp ("on", s) == 0)
        {
            if (strcmp ("*", val) != 0)
            {
                item.on = strdup (val);
            }
        }
        else if (strcmp ("to", s) == 0)
        {
            if (strcmp ("*", val) != 0)
            {
                item.to = strdup (val);
            }
        }
        else if (strcmp ("by", s) == 0)
        {
            if (strcmp ("*", val) != 0)
            {
                item.by = strdup (val);
            }
        }
        else if (strcmp ("is", s) == 0)
        {
            item.is = strdup (val);
        }
        else if (strcmp ("do", s) == 0)
        {
            if (++nb_used >= nb_alloc)
            {
                nb_alloc += 5;
                cmd = realloc (cmd, sizeof (*cmd) * (size_t) nb_alloc);
            }
            cmd[nb_used - 1] = strdup (val);
        }
        
        next:
        if (e)
        {
            s = e + 1;
        }
        else
        {
            break;
        }
    }
    if (is_item)
    {
        add_item (&item, nb_used, cmd);
    }
    
    free (data);
    free (cmd);
    return 1;
}

static void
free_react (void)
{
    react_t *r;
    int i;
    while (react)
    {
        free (react->on);
        free (react->to);
        free (react->by);
        free (react->is);
        if (react->regex)
        {
            g_regex_unref (react->regex);
        }
        i = sizeof (react->cmd) / sizeof (react->cmd[0]);
        for ( ; i >= 0; --i)
        {
            free (react->cmd[i]);
        }
        free (react->cmd);
        r = react;
        react = react->next;
        free (r);
    }
}

static char *
str_replace (const char *source, const char *replacements[])
{
    char  *s;
    int    i;
    int    l = sizeof (replacements) / sizeof (replacements[0]);
    
    char  *str;
    size_t alloc;
    size_t length;
    
    str = strdup (source);
    length = strlen (str);
    alloc = length;
    
    for (i = 0; i < l; i += 2)
    {
        if ((s = strstr (str, replacements[i])))
        {
            size_t len_match       = strlen (replacements[i]);
            size_t len_replacement = strlen (replacements[i + 1]);
            
            /* make sure there's room */
            if (length + (len_replacement - len_match) >= alloc)
            {
                char *s = str;

                alloc += len_replacement + 255;
                if (!(str = realloc (str, sizeof (*str) * alloc)))
                {
                    free (s);
                    return NULL;
                }
            }
            /* move what's after the match */
            memmove (s + len_replacement, s + len_match, strlen (s + len_match));
            /* replace match with replacement */
            memcpy (s, replacements[i + 1], len_replacement);
        }
    }
    
    return str;
}

static void
process_signal (const char *on, const char *to, const char *by,
                const char *msg, struct t_gui_buffer *buffer)
{
    react_t *r;
    GError *error = NULL;
    GMatchInfo *match_info;
    char **command;
    char *cmd;
    char *s;
    
    for (r = react; r; r = r->next)
    {
        /* check server */
        if (r->on && !weechat_string_match (on, r->on, 0))
        {
            continue;
        }
        /* check channel */
        if (r->to && !weechat_string_match (to, r->to, 0)
            /* if by is set, this is an incoming message. if r->to is "-"
             * and destination (to) doesn't start with a # (i.e. is not a
             * channel), it is indeed a PM to us, and a match after all */
            && !(by && r->to[0] == '-' && r->to[1] == '\0' && to[0] != '#'))
        {
            continue;
        }
        /* check author -- if by is NULL, we're sending this out */
        if (r->by && (
                (!by && !(r->by[0] == '-' && r->by[1] == '\0'))
             || ( by && !weechat_string_match (by, r->by, 0))
            ))
        {
            continue;
        }
        /* check message */
        if (r->is)
        {
            if (!r->regex)
            {
                r->regex = g_regex_new (r->is, G_REGEX_CASELESS | G_REGEX_OPTIMIZE,
                                        0, &error);
                if (error)
                {
                    weechat_printf (NULL, "%sFailed to compile regex: %s",
                                    weechat_prefix ("error"), error->message);
                    g_clear_error (&error);
                    r->regex = NULL;
                    continue;
                }
            }
            if (!g_regex_match (r->regex, msg, 0, &match_info))
            {
                g_match_info_free (match_info);
                continue;
            }
        }
        /* check regex if needed & process commands */
        for (command = r->cmd; *command; ++command)
        {
            if (r->is)
            {
                const char *rep[6] = { "$on", on, "$to", to, "$by", by };
                gboolean has_references = FALSE;
                /* are there back-references needing to be replaced? */
                if (g_regex_check_replacement (*command, &has_references, NULL)
                    && has_references)
                {
                    s = g_match_info_expand_references (match_info, *command, &error);
                    if (error)
                    {
                        weechat_printf (NULL, "%sFailed to expand references: %s",
                                        weechat_prefix ("error"), error->message);
                        g_clear_error (&error);
                        continue;
                    }
                }
                else
                {
                    s = *command;
                }
                
                if (!(cmd = str_replace (s, rep)))
                {
                    if (has_references)
                    {
                        g_free (s);
                    }
                    continue;
                }
                if (has_references)
                {
                    g_free (s);
                }
                weechat_command (buffer, cmd);
                free (cmd);
            }
            else
            {
                weechat_command (buffer, *command);
            }
        }
        g_match_info_free (match_info);
    }
}

static int
msg_in_cb (void *data _UNUSED_, const char *signal, const char *type_data _UNUSED_,
           void *signal_data)
{
    /*  signal          server,irc_in2_PRIVMSG
     *  signal_data     :nick!host PRIVMSG #chan :message
     */
    
    char buf[BUF_SIZE]; /* will hold on, to & by */
    char *on;
    char *to = NULL;
    char *by;
    char *s, *ss;
    int i;
    struct t_gui_buffer *buffer;
    
    /* quick regex to remove color codes & whatnot */
    gchar *msg;
    if (!(msg = g_regex_replace (regex_strip_color, signal_data, -1, 0, "", 0, NULL)))
    {
        return WEECHAT_RC_ERROR;
    }

    /* on: server name */
    on = buf;
    for (s = (char *) signal, i = 0; *s != ','; ++s, ++i)
    {
        on[i] = *s;
    }
    /* we put a dot for now, to get server.channel since that's the string
     * we need to search for the irc buffer. we'll turn it into a NULL then */
    on[i] = '.';
    
    /* locate space before PRIVMSG */
    if (!(s = strchr (msg, ' ')))
    {
        /* should not happen */
        g_free (msg);
        return WEECHAT_RC_ERROR;
    }
    /* 9 = strlen (" PRIVMSG ") */
    ss = s + 9;
    if (*ss == '#')
    {
        /* to: channel/nick */
        to = &on[i + 1];
        for (s = ss, i = 0; *s != ' '; ++s, ++i)
        {
            to[i] = *s;
        }
        to[i] = '\0';
        s = to - 1;
    }
    else
    {
        /* by: nick */
        by = &on[i + 1];
        for (s = msg + 1, i = 0; *s != '!'; ++s, ++i)
        {
            by[i] = *s;
        }
        by[i] = '\0';
        s = by - 1;
    }
    /* get buffer to send commands to. in buf we now have server.channel which
    * is the name of the irc buffer */
    buffer = weechat_buffer_search ("irc", buf);
    /* go turn the dot into a NULL, to properly end on (server name) */
    *s = '\0';
    if (to)
    {
        /* by: nick */
        by = &to[i + 1];
        for (s = msg + 1, i = 0; *s != '!'; ++s, ++i)
        {
            by[i] = *s;
        }
        by[i] = '\0';
    }
    else
    {
        /* to: channel/nick */
        to = &by[i + 1];
        for (s = ss, i = 0; *s != ' '; ++s, ++i)
        {
            to[i] = *s;
        }
        to[i] = '\0';
    }
    
    process_signal (on, to, by, strchr (msg + 1, ':') + 1, buffer);
    g_free (msg);
    return WEECHAT_RC_OK;
}

static int
msg_out_cb (void *data _UNUSED_, const char *signal, const char *type_data _UNUSED_,
            void *signal_data)
{
    /*  signal          server,irc_out_PRIVMSG
     *  signal_data     PRIVMSG #chan :message
     */
    
    char  buf[BUF_SIZE];
    char *s;
    char *on;
    char *to;
    int i;
    struct t_gui_buffer *buffer;
    
    /* on: server name */
    on = buf;
    for (s = (char *) signal, i = 0; *s != ','; ++s, ++i)
    {
        on[i] = *s;
    }
    /* we put a dot for now, to get server.channel since that's the string
     * we need to search for the irc buffer. we'll turn it into a NULL then */
    on[i] = '.';
    
    /* to: channel/nick */
    to = &on[i + 1];
    /* 8 = strlen ("PRIVMSG ") */
    for (s = (char *) signal_data + 8, i = 0; *s != ' '; ++s, ++i)
    {
        to[i] = *s;
    }
    to[i] = '\0';
    
    /* get buffer to send commands to. in buf we now have server.channel which
     * is the name of the irc buffer */
    buffer = weechat_buffer_search ("irc", buf);
    /* go turn the dot into a NULL, to properly end on (server name) */
    *(to - 1) = '\0';
    
    process_signal (on, to, NULL, strchr (signal_data + 1, ':') + 1, buffer);
    return WEECHAT_RC_OK;
}

static int
tobuffer_cb (void *data _UNUSED_, struct t_gui_buffer *buffer _UNUSED_,
             int argc, char **argv, char **argv_eol)
{
    struct t_gui_buffer *dest_buffer;
    char *s;
    
    if (argc < 3)
    {
        return WEECHAT_RC_ERROR;
    }
    
    if (!(s = strchr (argv[1], '.')))
    {
        weechat_printf (NULL, "%sBuffer name must be plugin.buffer",
                        weechat_prefix ("error"));
        return WEECHAT_RC_ERROR;
    }
    *s = '\0';
    if (!(dest_buffer = weechat_buffer_search (argv[1], s + 1)))
    {
        weechat_printf (NULL, "%sBuffer %s for plugin %s not found",
                        weechat_prefix ("error"), s + 1, argv[1]);
        return WEECHAT_RC_ERROR;
    }
    weechat_command (dest_buffer, argv_eol[2]);
    
    return WEECHAT_RC_OK;
}

static int
reload_cb (void *data _UNUSED_, struct t_gui_buffer *buffer _UNUSED_,
           const char *command _UNUSED_)
{
    free_react ();
    load_config ();
    return WEECHAT_RC_OK;
}

int
weechat_plugin_init (struct t_weechat_plugin *plugin,
                     int argc _UNUSED_,
                     char *argv[] _UNUSED_)
{
    weechat_plugin = plugin;
    
    load_config ();
    
    /* hook to message PRIVMSG */
    weechat_hook_signal ("*,irc_in2_privmsg", &msg_in_cb, NULL);
    
    /* hook to message PRIVMSG going out */
    weechat_hook_signal ("*,irc_out_privmsg", &msg_out_cb, NULL);
    
    /* hook to command reload so we reload our config as well */
    weechat_hook_command_run ("/reload", &reload_cb, NULL);
    
    /* hook command tobuffer to send commands to a specific buffer */
    weechat_hook_command ("tobuffer", "Send command to specified buffer",
                          "BUFFER COMMAND",
                          "BUFFER: buffer to send command to (plugin.buffer)\n"
                          "COMMAND: command (w/ args) to send",
                          "%(buffers_plugins_names)", &tobuffer_cb, NULL);
    
    /* regex to remove color codes & whatnot */
    regex_strip_color = g_regex_new ("(\\x1f|\\x02|\\x03|\\x16|\\x0f)(?:\\d{1,2}(?:,\\d{1,2})?)?",
                                     G_REGEX_CASELESS | G_REGEX_OPTIMIZE, 0, NULL);
    
    return WEECHAT_RC_OK;
}

int
weechat_plugin_end (struct t_weechat_plugin *plugin _UNUSED_)
{
    free_react ();
    if (regex_strip_color)
    {
        g_regex_unref (regex_strip_color);
    }
    return WEECHAT_RC_OK;
}
