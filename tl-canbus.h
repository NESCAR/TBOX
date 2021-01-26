#ifndef TLCANBUS_H
#define TLCANBUS_H
#include <glib.h>



#define DRIVER_ID     0x0001
#define LOCK_START_ID 0x0002
#define LOCK_END_ID   0x0003
#define AUTH_ACK_ID   0x0004
#define Req_AUTH_ID   0x0005

#define TREM_ACK_ID   0x0006
#define CONT_ACK_ID   0x0007

typedef struct _TLCANBusSendMsg
{
     guint32 wait_ack_id;
     GSList *msg_list;
}TLCANBusSendMsg;

gboolean tl_canbus_init(gboolean use_vcan);
void tl_canbus_uninit();
void tl_canbus_send_msg_add(gpointer frame);
void lock_auth_msg_can_send();
void lock_auth_msg_set(guint8* msg);
#endif // TLCANBUS_H
