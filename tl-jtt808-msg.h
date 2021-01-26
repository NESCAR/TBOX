#ifndef TLJTT808MSG_H
#define TLJTT808MSG_H
#include <glib.h>
#include <string.h>
#include <jtt808.h>

#ifndef WORD
typedef guint16 WORD;
#endif

#ifndef DWORD
typedef guint32 DWORD;
#endif

#ifndef BYTE
typedef guint8 BYTE;
#endif

#ifndef BCD
typedef guint8 BCD;
#endif

#define TermGeneResID 0x0001
#define ServGeneResID 0x8001
#define TermHeartbearID 0x0002
#define TermAuthID 0x0102
#define LocationMsgUpID 0x0200
#define LockAuthMsgID 0x8F00
#define LockAuthResID 0x0F07


#define TermAuthCode "DEJTNP"

typedef struct _LocationMsgUp{
    DWORD alarm;
    DWORD status;
    DWORD longitude;
    DWORD latitude;
    WORD  altiyude;
    WORD  speed;
    WORD  direction;
    BCD   time[6];
}LocationMsgUp;
typedef struct _LockAuthMsg{
    BCD  driver[6];
    BCD  lock_start[6];
    BCD  lock_end[6];
}LockAuthMsg;

static inline void LocationMsgUpToBE(const LocationMsgUp *msg, LocationMsgUp *be_msg)
{
    be_msg->alarm = GUINT32_TO_BE(msg->alarm);
    be_msg->status = GUINT32_TO_BE(msg->status);
    be_msg->longitude = GUINT32_TO_BE(msg->longitude);
    be_msg->latitude = GUINT32_TO_BE(msg->latitude);
    be_msg->altiyude = GUINT16_TO_BE(msg->altiyude);
    be_msg->altiyude = GUINT16_TO_BE(msg->altiyude);
    be_msg->speed = GUINT16_TO_BE(msg->speed);
    be_msg->direction = GUINT16_TO_BE(msg->direction);
    memcpy(be_msg->time, msg->time,6);
}

void term_auth_add();
void location_msg_add();
void lock_auth_res_add(LockAuthMsg *msg, guint8 *updata_time);
void resp_msg_add(MsgHeader *msg_head, CommonReplyResult res);

void serv_gene_resp_rev(guint8* binary_seq, int len);
void lock_auth_msg_rev(guint8* binary_seq, int len);
void lock_auth_msg_rev(guint8* msg_body, int len);

#endif // TLJTT808MSG_H
