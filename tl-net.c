#include <glib.h>
#include <gio/gio.h>
#include<tl-net.h>

#define PHONE "768901005626"
#define UP_MSG_TIME 5000
//#define IP "192.168.3.15"
#define IP "47.98.47.221"
#define PORT 6809
#define MAX_THREAD_NUM 20
#define REPLAY_CONUNT 2
#define REPLAY_TIME 1000

GByteArray *tl_net_read_buffer;
guint16 tl_net_flow_id;
GHashTable *tl_net_msg_table;
GThreadPool *thread_pool;
GAsyncQueue *tl_net_msg_send_queue;
typedef struct msg{
    guint16 seq;
    guchar *data;
}msg;

static void  tl_net_free_byte_array(GByteArray * user_data)
{
    g_byte_array_free(user_data, TRUE);
}
void print_hex(const GByteArray* garray)
{
    for (size_t i = 0; i < garray->len; i++)
        g_print("%02x", garray->data[i]);
    g_print("\n");
}
/*
* one message one thread or one update(maybe inlcude many messages) one thread
* here we use the first, but the later is more efficient
*
*/
gboolean send_jt808_msg(gpointer str, gpointer data)
{
     g_message("send_jt808_msg");
     GByteArray *gbarray = (GByteArray *)str;
     int id_start_index = gbarray->len -4;
     gint id=  *(gint*)(gbarray->data+id_start_index);
     g_message("id: %d",id);

     g_async_queue_lock(tl_net_msg_send_queue);
     GByteArray *gbarray_send = g_byte_array_new_take(gbarray->data,id_start_index);
     print_hex(gbarray_send);
     g_async_queue_push_unlocked(tl_net_msg_send_queue,gbarray_send);
     g_async_queue_unlock(tl_net_msg_send_queue);
//   g_byte_array_free (gbarray, TRUE);
}

gpointer thread_send_msg(gpointer data)
{
    GOutputStream * out_stream = g_io_stream_get_output_stream(G_IO_STREAM(data));

    while(1) {
         g_async_queue_lock(tl_net_msg_send_queue);
         GByteArray *gbarray_send = ( GByteArray *)g_async_queue_pop_unlocked(tl_net_msg_send_queue);
         gssize ret_int = g_output_stream_write(out_stream, gbarray_send->data, gbarray_send->len, NULL, NULL);
         g_output_stream_flush(out_stream, NULL, NULL);
         g_byte_array_free (gbarray_send, TRUE);
         g_async_queue_unlock(tl_net_msg_send_queue);
    }

}
gboolean tl_net_update_all_msg(gpointer user_data)
{
    g_message("tl_net_update_all_msg");
    GHashTable *msg_table = ( GHashTable *)user_data;
    GHashTableIter iter;
    gint *meg_id;
    GByteArray *msg_data;
    guchar binarySeq[17];
    g_hash_table_iter_init(&iter, msg_table);

    while(g_hash_table_iter_next(&iter,(gpointer *)&meg_id, (gpointer *)&msg_data))
    {
        PackageData packageData;
        packageData.msgHeader.msgId = (guint16)(*meg_id);

        packageData.msgHeader.msgBodyProperties.reservedBit=0;
        packageData.msgHeader.msgBodyProperties.hasSubPackage=false;
        packageData.msgHeader.msgBodyProperties.encryptionType=NONE;
        packageData.msgHeader.msgBodyProperties.msgLenth= msg_data->len;

        g_stpcpy(packageData.msgHeader.terminalPhone,PHONE);
        packageData.msgHeader.flowId = tl_net_flow_id;

        g_message("meg_id1");
         print_hex(msg_data);
        EncodeForMsgHeader(&packageData,binarySeq);
        g_byte_array_prepend(msg_data,(guint8*)binarySeq,12);

        // add empty checksum
        g_byte_array_append(msg_data,(guint8*)"a",1);
        // set checksum
        SetCheckSum(msg_data->data,msg_data->len);
        g_message("meg_id");
         print_hex(msg_data);
        //Coder
        gint binarySeqResLen  = msg_data->len*2+2; // max len
        guchar * binarySeqRes = (guchar *)g_strnfill(binarySeqResLen,'\0');
        DoEscapeForSend(msg_data->data,binarySeqRes,msg_data->len,&binarySeqResLen);

        // add to pool
//        GByteArray *gbarray = g_byte_array_new_take(binarySeqRes,binarySeqResLen);// will free binarySeqRes
//        GHashTable *jt808_msg_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, tl_net_free_byte_array);
//        gint *p_flow_id = g_new(gint,1);
//        *p_flow_id = tl_net_flow_id;
//        g_hash_table_insert(jt808_msg_table,p_flow_id,gbarray);
        gint *p_flow_id = g_new(gint,1);
        *p_flow_id = tl_net_flow_id;

        GByteArray *gbarray = g_byte_array_new_take(binarySeqRes,binarySeqResLen);// will free binarySeqRes

        g_byte_array_append(gbarray,(guint8 *)p_flow_id, sizeof(gint));

        g_thread_pool_push(thread_pool, (gpointer)gbarray, NULL);
        g_hash_table_iter_remove(&iter);
        tl_net_flow_id++;
    }
    return TRUE;
}
gboolean tl_net_test(gpointer user_data)
{
    GByteArray *gbarray = g_byte_array_new();
    g_byte_array_append(gbarray,(guint8*)"abcd",4);
    tl_net_add_msg(10,gbarray);
    return TRUE;
}

gboolean tl_net_init()
{
    // net
    g_message("net begin");
    GError *error = NULL;
    tl_net_flow_id=0x06E1;
    GSocketClient * client = g_socket_client_new();
    GSocketConnection * connection = g_socket_client_connect_to_host (client,IP,PORT,NULL,&error);
    if (error){
        g_error("Error: %s\n", error->message);
    }else{
        g_message("Connection ok");
    }
    //read_msg
    GSocket *socket = g_socket_connection_get_socket(connection);
    gint fd = g_socket_get_fd(socket);
    GIOChannel *channel = g_io_channel_unix_new(fd);
    if (channel) {
        g_io_add_watch(channel, G_IO_IN, (GIOFunc)tl_net_read_msg, connection);
        g_io_channel_unref(channel);
    }
    thread_pool = g_thread_pool_new(send_jt808_msg, NULL, MAX_THREAD_NUM, TRUE, NULL);
    tl_net_msg_table = g_hash_table_new_full(g_int_hash, g_int_equal, g_free, tl_net_free_byte_array);
    tl_net_msg_send_queue = g_async_queue_new();

    GThread *gthread = g_thread_new ("send_msg", thread_send_msg, connection);
//    g_thread_join(gthread);

    g_timeout_add(UP_MSG_TIME,tl_net_update_all_msg,tl_net_msg_table);
   // g_timeout_add(1000,tl_net_test,NULL);
    g_message("tl_net_init is ok");
    tl_net_read_buffer= g_byte_array_new();
    GByteArray *gbarray = g_byte_array_new();
    g_byte_array_append(gbarray,(guint8*)"DEJTNP",6);
    tl_net_add_msg(0x0102,gbarray);
    return TRUE;
}

void tl_net_add_msg(gint id, GByteArray *msg)
{
    g_message("tl_net_add_msg");
    gint *p_id = g_new(gint,1);
    *p_id = id;
    g_hash_table_insert(tl_net_msg_table,p_id,msg);
}

gboolean tl_net_read_msg(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    gchar buffer[1000];
    gsize len = 0;
    GError *error=NULL;
    g_message("read_msg");
    //GIOStatus ret = g_io_channel_read_line(channel,&buffer,&len,NULL,&error);
    GIOStatus ret = g_io_channel_read_chars (channel,buffer,100,&len,&error);
    if (ret == G_IO_STATUS_ERROR){
        g_error ("Error reading: %s\n", error->message);
        g_object_unref(data);
        return FALSE;
    }
    gint raw_msg_start,raw_msg_len, msg_len;


    if(len>0)
    {
       g_byte_array_append(tl_net_read_buffer,buffer,len);
       while(tl_net_split_msg(tl_net_read_buffer,&raw_msg_start,&raw_msg_len))
       {

           DoEscapeForReceive(tl_net_read_buffer->data+raw_msg_start,buffer,raw_msg_len,&msg_len);
           if(Validate(buffer,msg_len))
           {


           }
           g_byte_array_remove_range(tl_net_read_buffer,raw_msg_start,raw_msg_len);
       }
        g_message("tl_net_read_buffer:%d",tl_net_read_buffer)
        g_message("rev: %s\n", tl_net_read_buffer);
        g_message("strlen(tl_net_read_buffer):%d",strlen(tl_net_read_buffer));
    }
    return TRUE;
}

gboolean tl_net_split_msg(GByteArray* buffer,gint *msg_start, gint *msg_len)
{
    gint index[2];
    int index_count=0;
    for(int i=0;i<buffer->len;i++)
    {
        if(buffer[i]==0x7E)
        {
            index[index_count] = i;
            index_count++;
            if(2==index_count)
            {
                *msg_start = index[0];
                *msg_len = index[1]-index[0]+1;
                return TRUE;
            }
        }
    }
    return FALSE;

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
