#include <linux/can.h>
#include <string.h>

#include<tl-jtt808-msg.h>
#include<tl-gps.h>
#include<tl-canbus.h>
extern GHashTable  *tl_net_msg_send_table;
extern GHashTable  *tl_net_msg_ack_table;
extern GMutex       tl_net_msg_ack_mutex;
/* send  jtt808 msg */

void term_auth_add(){
     GByteArray *gbarray = g_byte_array_new();
     gint *id = g_new(gint,1);
     *id = TermAuthID;
     g_byte_array_append(gbarray,(guint8 *)TermAuthCode,strlen(TermAuthCode));
     g_hash_table_insert(tl_net_msg_send_table,id,gbarray);
}
void location_msg_add(){
    GByteArray *gbarray = g_byte_array_new();
    gint *id = g_new(gint,1);
    *id = LocationMsgUpID;
    tl_gps_message_get(gbarray);
    g_hash_table_insert(tl_net_msg_send_table,id,gbarray);
}
void resp_msg_add(MsgHeader *msg_head, CommonReplyResult res)
{
    GByteArray *gbarray = g_byte_array_new();
    gint *id = g_new(gint,1);
    *id = TermGeneResID;
    CommonRespMsgBody resp_msg;
    resp_msg.replyId = msg_head->msgId;
    resp_msg.replyFlowId = msg_head->flowId;
    resp_msg.replyCode = res;
    guint8 bf[5];
    EncodeForCRMB(&resp_msg,bf);
    g_byte_array_append(gbarray,bf,5);
    g_hash_table_insert(tl_net_msg_send_table,id,gbarray);
}
void lock_auth_res_add(LockAuthMsg *msg, guint8 *updata_time)
{
    GByteArray *gbarray = g_byte_array_new();
    gint *id = g_new(gint,1);
    *id = LockAuthResID;

    g_byte_array_append(gbarray,(guint8 *)msg,sizeof(LockAuthMsg));
    g_byte_array_append(gbarray,(guint8 *)updata_time,6);
    g_hash_table_insert(tl_net_msg_send_table,id,gbarray);
}

/* receive  jtt808 msg */
void serv_gene_resp_rev(guint8* msg_body, int len)
{
    CommonRespMsgBody crmb;
    DecodeForCRMB(&crmb, msg_body);
    gint flow_id = crmb.replyFlowId;
    g_mutex_lock(&tl_net_msg_ack_mutex);
    GCond *cond =(GCond *)g_hash_table_lookup(tl_net_msg_ack_table,&flow_id);
    if(cond!=NULL)
    {
        g_cond_signal(cond);
        g_message(" 808 resp g_cond_signal,flow_id: %d",flow_id);
    }
    g_mutex_unlock(&tl_net_msg_ack_mutex);
}

void print_hex_can(guint8* can_data)
{
    GByteArray *garray_str=g_byte_array_new();
    for(int i=0;i<8;i++)
    {
       char* str= g_strdup_printf("%02x ",can_data[i]);
       g_byte_array_append(garray_str,(guint8 *)str,3);
    }
    g_message("%s",garray_str->data);
    g_byte_array_free(garray_str,TRUE);

}
void lock_auth_msg_rev(guint8* msg_body, int len)
{
    lock_auth_msg_set(msg_body);

    lock_auth_msg_can_send();
}


