#ifndef TLNET_H
#define TLNET_H
#include <glib.h>
#include <gio/gio.h>
gboolean tl_net_read_msg(GIOChannel *channel, GIOCondition condition, gpointer data);
gboolean tl_net_init();
#endif // TLNET_H
