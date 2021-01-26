#include <glib.h>
#include <gio/gio.h>
#include<tl-net.h>
#include<tl-jtt808-msg.h>
#define PHONE "768901005626"

//ms
#define UP_MSG_TIME 5000
//#define IP "192.168.3.23"
#define IP "47.98.47.221"
#define PORT 6809
#define MAX_THREAD_NUM 20
#define REPLAY_CONUNT 1
//ms
#define REPLAY_TIME 2000

GIOChannel *tl_net_channel;

GByteArray  *tl_net_read_buffer;
guint16     tl_net_flow_id;
GHashTable  *tl_net_msg_send_table;
GHashTable  *tl_net_msg_ack_table;
GMutex      tl_net_msg_ack_mutex;
GThreadPool *thread_pool;
GAsyncQueue *tl_net_msg_send_queue;

typedef struct _SendMsgType{
    gint msg_id;
    gint flow_id;
    GByteArray *garray;
}SendMsgType;

static void  tl_net_free_byte_array(GByteArray * user_data)
{
    g_byte_array_free(user_data, TRUE);
}

/* print GByteArray with Hex
 *
*/
void print_hex(const GByteArray* garray)
{
    GByteArray *garray_str=g_byte_array_new();
    g_message("print_hex size: %d:",garray->len);
    for (size_t i = 0; i < garray->len; i++)
    {
        char* str= g_strdup_printf("%02x ", garray->data[i]);
        g_byte_array_append(garray_str,(guint8 *)str,3);
    }
        g_message("%s",garray_str->data);
        g_byte_array_free(garray_str,TRUE);
}

/*
* one message one thread or one update(maybe inlcude many messages) one thread
* here we use the first, but the later is more efficient
*
*/
gboolean send_jt808_msg(gpointer str, G_GNUC_UNUSED gpointer data)
{

     g_message("send_jt808_msg");
     SendMsgType * send_msg =( SendMsgType *)str;
     GByteArray *gbarray = send_msg->garray;

     gint *flow_id = g_new(gint,1);
     *flow_id =send_msg->flow_id;
     print_hex(gbarray);

//     g_message("id----: %d",*flow_id);
//     g_message("id_start_index----: %d",id_start_index);
//     print_hex(gbarray);
//     g_message("id_start_index----: %d",*flow_id);

     // termina general response, don't need response
     if(TermGeneResID == send_msg->msg_id)
     {
         g_async_queue_lock(tl_net_msg_send_queue);
         g_async_queue_push_unlocked(tl_net_msg_send_queue,gbarray);
         g_async_queue_unlock(tl_net_msg_send_queue);
     }
     else
     {
         GCond * cond = g_new(GCond,1);
         g_cond_init(cond);
         g_mutex_lock(&tl_net_msg_ack_mutex);
         g_hash_table_insert(tl_net_msg_ack_table,flow_id,cond);
         g_mutex_unlock(&tl_net_msg_ack_mutex);


         for(int i=0;i<REPLAY_CONUNT;i++)
         {
             g_async_queue_lock(tl_net_msg_send_queue);
             g_async_queue_push_unlocked(tl_net_msg_send_queue,gbarray);
             g_async_queue_unlock(tl_net_msg_send_queue);

             gint64 end_time = g_get_monotonic_time () +  REPLAY_TIME* G_TIME_SPAN_MILLISECOND;
             g_mutex_lock(&tl_net_msg_ack_mutex);

             if(!g_cond_wait_until(cond,&tl_net_msg_ack_mutex,end_time)){
                 // time out passed
                 g_mutex_unlock(&tl_net_msg_ack_mutex);
                 g_message("wait 808 ack time out passed");
                 continue;
             }
             g_cond_clear(cond);
             g_hash_table_remove(tl_net_msg_ack_table,flow_id);
             g_mutex_unlock(&tl_net_msg_ack_mutex);
             g_message("808 wait ack ok");
             break;
         }
     }
     g_free(send_msg);

     return TRUE;
}

//send thread
gpointer thread_send_msg(gpointer data)
{
    GIOChannel *channel = (GIOChannel *)data;
    GByteArray *gbarray_send=NULL;

    while(1){
         g_async_queue_lock(tl_net_msg_send_queue);
         gbarray_send= ( GByteArray *)g_async_queue_pop_unlocked(tl_net_msg_send_queue);
//         g_message("g_async_queue_lock-begin");
//         print_hex(gbarray_send);
//         g_message("g_async_queue_lock-end");
         if(gbarray_send->data) g_io_channel_write_chars(channel,(gchar*)gbarray_send->data,gbarray_send->len,NULL,NULL);
         g_io_channel_flush(channel,NULL);
         g_byte_array_free (gbarray_send, TRUE);
         g_async_queue_unlock(tl_net_msg_send_queue);
    }
}
/* socket io receive
 *
 */
gboolean tl_net_update_receive_msg(guint8* binary_seq, int len)
{

    PackageData pack_data;
    DecodeForMsgHeader(binary_seq,&pack_data,len);
//   int msg_id = GetMsgID(binary_seq,&msg_id);
     int msg_id = pack_data.msgHeader.msgId;
     int body_len = pack_data.msgHeader.msgBodyProperties.msgLenth;
    g_message("binary_seq size: %d, [0]:%02x,[1]%02x",len,binary_seq[0],binary_seq[1]);
    g_message("msg_id: %x",pack_data.msgHeader.msgId);


    switch (msg_id) {
    case ServGeneResID:
    {
        serv_gene_resp_rev(pack_data.msgBody,body_len);
//        CommonRespMsgBody crmb;

//        DecodeForCRMB(&crmb, binary_seq);
//        gint flow_id = crmb.replyFlowId;
//        g_mutex_lock(&tl_net_msg_ack_mutex);
//        gpointer *cond = g_hash_table_lookup(tl_net_msg_ack_table,&flow_id);
//        if(cond!=NULL)
//        {
//            g_cond_signal(cond);
//            g_message("g_cond_signal(cond)");
//        }
//        g_mutex_unlock(&tl_net_msg_ack_mutex);
        break;
    }
    case LockAuthMsgID:
    {
        lock_auth_msg_rev(pack_data.msgBody,body_len);
        resp_msg_add(&pack_data.msgHeader,CRR_SUCCESS);
        break;
    }
    default:
        resp_msg_add(&pack_data.msgHeader,CRR_UNSUPPORTED);
        break;
    }
    return TRUE;
}

/* timeout or immedita
 *
 */
gboolean tl_net_update_send_msg(gpointer user_data)
{
    //location_msg_add();
    g_message("tl_net_update_all_msg");
    GHashTable *msg_table = ( GHashTable *)user_data;
    GHashTableIter iter;
    gint *msg_id;
    GByteArray *msg_data;
    guchar binarySeq[17];
    g_hash_table_iter_init(&iter, msg_table);

    while(g_hash_table_iter_next(&iter,(gpointer *)&msg_id, (gpointer *)&msg_data))
    {
        PackageData packageData;
        packageData.msgHeader.msgId = (guint16)(*msg_id);

        packageData.msgHeader.msgBodyProperties.reservedBit=0;
        packageData.msgHeader.msgBodyProperties.hasSubPackage=false;
        packageData.msgHeader.msgBodyProperties.encryptionType=NONE;
        packageData.msgHeader.msgBodyProperties.msgLenth= msg_data->len;

        g_stpcpy(packageData.msgHeader.terminalPhone,PHONE);
        packageData.msgHeader.flowId = tl_net_flow_id;

        g_message("raw meg_data");
        print_hex(msg_data);
        EncodeForMsgHeader(&packageData,binarySeq);
        g_byte_array_prepend(msg_data,(guint8*)binarySeq,12);
        // add empty checksum
        g_byte_array_append(msg_data,(guint8*)"a",1);
        // set checksum
        SetCheckSum(msg_data->data,msg_data->len);

        //Coder
        gint binarySeqResLen  = msg_data->len*2+2; // max len
        guchar * binarySeqRes = (guchar *)g_strnfill(binarySeqResLen,'\0');
        DoEscapeForSend(msg_data->data,binarySeqRes,msg_data->len,&binarySeqResLen);



        GByteArray *gbarray = g_byte_array_new_take(binarySeqRes,binarySeqResLen);// will free binarySeqRes

        SendMsgType *send_msg = g_new(SendMsgType,1);
        send_msg->flow_id = tl_net_flow_id;
        send_msg->msg_id = *msg_id;
        send_msg->garray = gbarray;
        g_thread_pool_push(thread_pool, (gpointer)send_msg, NULL);
        g_hash_table_iter_remove(&iter);
        tl_net_flow_id++;
    }
    return TRUE;
}
//gboolean tl_net_test(gpointer user_data)
//{
//    GByteArray *gbarray = g_byte_array_new();
//    g_byte_array_append(gbarray,(guint8*)"abcd",4);
//    tl_net_add_msg(10,gbarray);
//    return TRUE;
//}

gboolean tl_net_init()
{
    // net
    g_message("net begin");
    GError *error = NULL;
    tl_net_flow_id=1;
    GSocketClient * client = g_socket_client_new();
    GSocketConnection * connection = g_socket_client_connect_to_host (client,IP,PORT,NULL,&error);
    if (error){
        g_error("Error: %s\n", error->message);
    }else{
        g_message("Connection ok");
    }

    //watch io to read_msg
    GSocket *socket = g_socket_connection_get_socket(connection);
    gint fd = g_socket_get_fd(socket);
    tl_net_channel = g_io_channel_unix_new(fd);
    g_io_channel_set_encoding(tl_net_channel,NULL,NULL);
    if (tl_net_channel) {
        g_io_add_watch(tl_net_channel, G_IO_IN, (GIOFunc)tl_net_read_msg, connection);
//        g_io_channel_unref(channel);
    }
    thread_pool = g_thread_pool_new((GFunc)send_jt808_msg, NULL, MAX_THREAD_NUM, TRUE, NULL);
    tl_net_msg_send_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, (GDestroyNotify)tl_net_free_byte_array);
    tl_net_msg_ack_table = g_hash_table_new_full(g_int_hash, g_int_equal,g_free,g_free);

    tl_net_msg_send_queue = g_async_queue_new();

    g_thread_new ("send_msg", thread_send_msg, tl_net_channel);
//    g_thread_join(gthread);

    g_timeout_add(UP_MSG_TIME,tl_net_update_send_msg,tl_net_msg_send_table);
   // g_timeout_add(1000,tl_net_test,NULL);
    g_message("tl_net_init is ok");
    tl_net_read_buffer= g_byte_array_new();
    term_auth_add();
    return TRUE;
}
gboolean tl_net_split_jt808_msg(GByteArray* buffer,gint *raw_msg_start, gint *raw_msg_len)
{
    g_message("tl_net_split_jt808_msg");
    gint index[2];
    int index_count=0;
    for(int i=0;i<(gint)buffer->len;i++)
    {
        if(buffer->data[i]==0x7E)
        {
            index[index_count] = i;
            index_count++;
            if(2==index_count)
            {
                *raw_msg_start = index[0];
                *raw_msg_len = index[1]-index[0]+1;
                g_message("raw_msg_start; %d",*raw_msg_start);
                g_message("raw_msg_len; %d",*raw_msg_len);
                return TRUE;
            }
        }
    }
    return FALSE;
}
gboolean tl_net_read_msg(GIOChannel *channel, G_GNUC_UNUSED GIOCondition condition, G_GNUC_UNUSED gpointer data)
{

    guint8 buffer[100];
    gsize len = 0;
    GError *error=NULL;
    g_message("read_msg");

    GIOStatus ret = g_io_channel_read_chars (channel,(gchar *)buffer,100,&len,&error);
    if (ret == G_IO_STATUS_ERROR){
        g_error ("Error reading: %s\n", error->message);
        g_object_unref(data);
        return FALSE;
    }
    g_message("buffer len :%d",len);
    gint raw_msg_start,raw_msg_len, msg_len;
    if(len>0){
       g_byte_array_append(tl_net_read_buffer,buffer,len);
       g_message("tl_net_read_buffer");
       print_hex(tl_net_read_buffer);
    }
       while(tl_net_split_jt808_msg(tl_net_read_buffer,&raw_msg_start,&raw_msg_len)){
           // one msg write to buffer
           DoEscapeForReceive(tl_net_read_buffer->data+raw_msg_start,buffer,raw_msg_len,&msg_len);
           if(Validate(buffer,msg_len)){
               g_message("Validate");
               tl_net_update_receive_msg(buffer,msg_len);
           }
           else
           {

           }
           g_byte_array_remove_range(tl_net_read_buffer,raw_msg_start,raw_msg_len);
       }


    return TRUE;
}


//gboolean write_msg(GIOChannel *channel, GIOCondition condition, gpointer data)
//{
//    gsize len = 0;
//    gchar *buffer = NULL;
//    GSocketConnection * connection = (GSocketConnection *)data;
//    GError *error=NULL;

//    GIOStatus ret = g_io_channel_read_line(channel, &buffer, &len, NULL, NULL);
//    if (ret == G_IO_STATUS_ERROR){
//        g_error ("Error reading: %s\n", error->message);
//        g_object_unref(data);
//        return FALSE;
//    }
//    else if (ret == G_IO_STATUS_EOF) {
//        g_print("client finished\n");
//        return FALSE;
//    }
//    else {
//        if(len > 0) {
//            if ('\n' == buffer[len -1]) {
//                buffer[len -1] = '\0';
//            }
//            g_print("you are write: %s\n", buffer);
//        }
//        if(NULL != buffer) {
//            //判断结束符
//            if(strcasecmp(buffer, "q") == 0){
//                g_main_loop_quit(loop);
//            }
//        }

//        GOutputStream * out_stream = g_io_stream_get_output_stream(G_IO_STREAM(connection));
//        gssize ret_int = g_output_stream_write(out_stream, buffer, len, NULL, NULL);
//        g_output_stream_flush(out_stream, NULL, NULL);

//        if (ret_int < 1) {
//            g_error("write error");
//        }

//        g_free(buffer);

//        return TRUE;
//    }
//}
//int main(int argc, char *argv[])
//{

//    GError *error = NULL;
//    GSocketClient * client = g_socket_client_new();

//    GSocketConnection * connection = g_socket_client_connect_to_host (client,"127.0.0.1",4000,NULL,&error);
//    if (error){
//        g_error("Error: %s\n", error->message);
//    }else{
//        g_message("Connection ok");
//    }

//    //stdin->write_msg
//    GIOChannel* channel_stdin = g_io_channel_unix_new(1);
//    if(channel_stdin)
//    {
//        g_io_add_watch(channel_stdin, G_IO_IN, write_msg, connection);
//        g_io_channel_unref(channel_stdin);
//    }

//    //read_msg
//    GSocket *socket = g_socket_connection_get_socket(connection);
//    gint fd = g_socket_get_fd(socket);
//    GIOChannel *channel = g_io_channel_unix_new(fd);
//    if (channel) {
//        g_io_add_watch(channel, G_IO_IN, (GIOFunc)read_msg, connection);
//        g_io_channel_unref(channel);
//    }

//    loop = g_main_loop_new(NULL, FALSE);
//    g_main_loop_run(loop);

//    g_main_loop_unref(loop);

//    return TRUE;
//}
