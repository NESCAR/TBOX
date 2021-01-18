#ifndef TLCANBUS_H
#define TLCANBUS_H
#include <glib.h>

gboolean tl_canbus_init(gboolean use_vcan);
void tl_canbus_uninit();
#endif // TLCANBUS_H
