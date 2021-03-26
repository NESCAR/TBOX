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

#define TermGeneResID   0x0001
#define ServGeneResID   0x8001
#define TermHeartbearID 0x0002
#define TermAuthID      0x0102
#define LocationMsgUpID 0x0200
#define LockAuthMsgID   0x8F00
#define LockAuthResID   0x0F07
#define AxleMsgId       0x0F00

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

typedef struct _AxlxeLodeMsg{
    WORD all_load;
    WORD left_frist;
    WORD left_second;
    WORD left_third;
    WORD right_first;
    WORD right_second;
    WORD right_third;
    BCD  time[6];
}AxleLodeMsg;

/**
 *
 */
typedef struct _AllCanJt808Msg{
    AxleLodeMsg axle_load;
}AllCanJt808Msg;

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
static inline void AxleLoadMsgUpToBE(const AxleLodeMsg *msg, AxleLodeMsg *be_msg)
{
    be_msg->all_load = GUINT16_TO_BE(msg->all_load);
    be_msg->left_frist = GUINT16_TO_BE(msg->left_frist);
    be_msg->left_second = GUINT16_TO_BE(msg->left_second);
    be_msg->left_third = GUINT16_TO_BE(msg->left_third);

    be_msg->right_first = GUINT16_TO_BE(msg->right_first);
    be_msg->right_second = GUINT16_TO_BE(msg->right_second);
    be_msg->right_third = GUINT16_TO_BE(msg->right_third);
    memcpy(be_msg->time, msg->time,6);
}

void term_auth_add();
void location_msg_add();
void lock_auth_res_add(LockAuthMsg *msg, guint8 *updata_time);
void resp_msg_add(MsgHeader *msg_head, CommonReplyResult res);

void serv_gene_resp_rev(guint8* binary_seq, int len);
void lock_auth_msg_rev(guint8* binary_seq, int len);
void lock_auth_msg_rev(guint8* msg_body, int len);
void get_time_hex(guint8 *time);
#endif // TLJTT808MSG_H
