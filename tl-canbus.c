#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>

#include "tl-canbus.h"
#include "tl-parser.h"
#include "tl-jtt808-msg.h"

/*规定时间内没有接受到CAN数据帧 */
#define TL_CANBUS_NO_DATA_TIMEOUT 180

/* CAN数据帧重发的次数*/
#define CAN_REPLAY_CONUNT 1

/* CAN数据帧等待回复的时间*/
#define CAN_REPLAY_TIME   5000
LockAuthMsg tl_canbus_lock_auth_msg={0};
typedef struct _TLCANBusSocketData
{
    gchar *device;
    int fd;
    GIOChannel *channel;
    guint watch_id;
}TLCANBusSocketData;

typedef struct _TLCANBusData
{
    gboolean initialized;
    GHashTable *socket_table;
    gint64 data_timestamp;
    guint check_timeout_id;
    canid_t wait_ack_id;
    GAsyncQueue *msg_send_queue;
    GCond *ack_cond;
    GMutex *ack_mutex;
    GThread *thread_send_msg;
}TLCANBusData;

static TLCANBusData g_tl_canbus_data = {0};

/**
 * 用于关闭CAN bus时释放内存
 */
static void tl_canbus_socket_data_free(TLCANBusSocketData *data)
{
    if(data==NULL)
    {
        return;
    }
    if(data->watch_id>0)
    {
        g_source_remove(data->watch_id);
    }
    if(data->channel!=NULL)
    {
        g_io_channel_unref(data->channel);
    }
    if(data->fd>0)
    {
        close(data->fd);
    }
    g_free(data);
}

/**
 * 对接收的CAN数据帧进行检查处理
 * @param can_id 接收到的数据帧id
 * @param can_data CAN数据
 */
void tl_canbus_receive(const gchar* device, guint32 can_id, guint8* can_data,gsize len)
{
    if(can_id == g_tl_canbus_data.wait_ack_id)
    {
        g_mutex_lock(g_tl_canbus_data.ack_mutex);
        g_cond_signal(g_tl_canbus_data.ack_cond);
        g_mutex_unlock(g_tl_canbus_data.ack_mutex);
        g_message("get can ack, can id: %08x",can_id);
    }
    can_id = can_id & 0x1FFFFF;

    switch (can_id) {
    case AUTH_ACK_ID:
    {
        lock_auth_res_add(&tl_canbus_lock_auth_msg, can_data);
        break;
    }
    case Req_AUTH_ID:
    {
        lock_auth_msg_can_send();
        break;
    }
    default:
        tl_parser_parse_can_data(device,
            can_id, can_data, len);
        break;
    }
}

/**
 * CAN接收函数
 */
static gboolean tl_canbus_socket_io_channel_watch(GIOChannel *source,
    GIOCondition condition, gpointer user_data)
{
    TLCANBusSocketData *socket_data = (TLCANBusSocketData *)user_data;
//    struct canfd_frame frame;
    struct can_frame frame;
    ssize_t rsize;

    if(user_data==NULL)
    {
        return FALSE;
    }
    if(condition & G_IO_IN)
    {
        rsize = read(socket_data->fd, &frame, CAN_MTU);
        if(rsize>0)
        {
            if(rsize>=(ssize_t)CAN_MTU)
            {
                g_print("%s: %08x: %d\n",socket_data->device, frame.can_id,frame.can_dlc);
//                tl_parser_parse_can_data(socket_data->device,
//                    frame.can_id, frame.data, frame.len);
            gsize len;
            g_io_channel_write_chars(source,(gchar *)&frame,CAN_MTU,&len,NULL);
            g_io_channel_flush(source,NULL);
            g_message("CAN callback send len: %d",len);
            tl_canbus_receive(socket_data->device,frame.can_id, frame.data, frame.can_dlc);

            g_tl_canbus_data.data_timestamp = g_get_monotonic_time();
            }
            else
            {
                g_warning("TLCANBus received an incompleted packet "
                    "on device %s with size %ld", socket_data->device,
                    (long)rsize);
            }
        }
    }

    return TRUE;
}

/**
 * 打开对应CAN设备的socket
 * @param device CAN设备的名称，如can0，can1
 */
static gboolean tl_canbus_open_socket(const gchar *device)
{
    int fd;
    struct ifreq ifr;
    struct sockaddr_can addr;
    GIOChannel *channel;
    TLCANBusSocketData *socket_data;
    g_message("tl_canbus_open_socket,deviece name: %s",device);

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(fd < 0)
    {
        g_warning("TLCANBus Failed to open CAN socket %s: %s", device,
            strerror(errno));
        return FALSE;
    }
    strncpy(ifr.ifr_name, device, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);

    if(ifr.ifr_ifindex==0)
    {
        close(fd);
        g_warning("TLCANBus Failed to get interface index on "
            "device %s: %s", device, strerror(errno));
        return FALSE;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))<0)
    {
        close(fd);
        g_warning("TLCANBus Failed to bind CAN socket %s: %s", device,
            strerror(errno));
        return FALSE;
    }

    channel = g_io_channel_unix_new(fd);
    if(channel==NULL)
    {
        close(fd);
        g_warning("TLCANBus Failed to create IO channel!");
        return FALSE;
    }
    g_io_channel_set_encoding(channel,NULL,NULL);

    socket_data = g_new0(TLCANBusSocketData, 1);
    socket_data->fd = fd;
    socket_data->device = g_strdup(device);
    socket_data->channel = channel;

    socket_data->watch_id = g_io_add_watch(channel, G_IO_IN,
        tl_canbus_socket_io_channel_watch, socket_data);

    g_hash_table_replace(g_tl_canbus_data.socket_table,g_strdup(device),
        socket_data);

    return TRUE;
}

/**
 * @brief tl_canbus_scan_devices 扫描CAN设备
 * @param use_vcan, CAN设备的名称为can或者vcan
 * @return
 */
static GSList *tl_canbus_scan_devices(gboolean use_vcan)
{
    GSList *device_list = NULL;
    struct ifaddrs *addrs = NULL, *addrs_foreach;

    if(getifaddrs(&addrs)!=0)
    {
        g_warning("TLCANBus Cannot get CANBus interface data: %s",
            strerror(errno));
        return NULL;
    }

    for(addrs_foreach=addrs;addrs_foreach!=NULL;
        addrs_foreach=addrs_foreach->ifa_next)
    {

        if(!g_str_has_prefix(addrs_foreach->ifa_name, use_vcan ? "can" :
            "vcan"))
        {
            continue;
        }
        if(addrs_foreach->ifa_flags & IFF_UP)
        {
            device_list = g_slist_prepend(device_list,
                g_strdup(addrs_foreach->ifa_name));
        }
    }

    freeifaddrs(addrs);

    return device_list;
}

/**
 * @brief tl_canbus_check_timeout_cb CAN数据帧接收超时，规定时间内未收到CAN数据帧
 * @param user_data
 * @return
 */
static gboolean tl_canbus_check_timeout_cb(gpointer user_data)
{
    TLCANBusData *canbus_data = (TLCANBusData *)user_data;
    gint64 now;

    if(user_data==NULL)
    {
        return FALSE;
    }

    now = g_get_monotonic_time();

    if(now - canbus_data->data_timestamp >
        (gint64)TL_CANBUS_NO_DATA_TIMEOUT * 1e6)
    {
        g_message("TLCANBus no CANBus data received for 3min, "
            "start to shutdown.");
//        tl_main_request_shutdown();
    }

    return TRUE;
}
/**
 * @brief tl_canbus_send_msg_add 向CAN发送队列中添加数据
 * @param frame 添加的数据的类型为TLCANBusSendMsg
 */
void tl_canbus_send_msg_add(gpointer frame)
{
    g_async_queue_lock(g_tl_canbus_data.msg_send_queue);
    g_async_queue_push_unlocked(g_tl_canbus_data.msg_send_queue,frame);
    g_async_queue_unlock(g_tl_canbus_data.msg_send_queue);
}
/**
 * @brief tl_canbus_thread_send_msg CAN发送线程，读取CAN发送队列中数据，发送并等待回复
 * @param data
 * @return
 */
gpointer tl_canbus_thread_send_msg(gpointer data)
{
    TLCANBusSocketData *socket_data = ( TLCANBusSocketData *)data;
    GIOChannel *channel = socket_data->channel;
    struct can_frame *frame = NULL;
    TLCANBusSendMsg* send_msg = NULL;
    GSList *list_foreach = NULL;
    while(1)
    {
        g_async_queue_lock(g_tl_canbus_data.msg_send_queue);
        send_msg = (TLCANBusSendMsg*)g_async_queue_pop_unlocked(g_tl_canbus_data.msg_send_queue);
        g_async_queue_unlock(g_tl_canbus_data.msg_send_queue);
        if(send_msg==NULL) continue;
        for(int i=0;i<CAN_REPLAY_CONUNT;i++)
        {
            // msg_list为 can_frame 组成的链表，可以实现多个can_frame发送后等待一个ack信号
            for(list_foreach=send_msg->msg_list;list_foreach!=NULL;
                list_foreach=g_slist_next(list_foreach))
            {
                gsize len;
                frame=(struct can_frame *)list_foreach->data;
                g_io_channel_write_chars(channel,(gchar*)frame,CAN_MTU,&len,NULL);
                g_io_channel_flush(channel,NULL);
                g_message("CAN begin send len: %d",len);
                g_print("%s: %d: %d\n",socket_data->device, frame->can_id,frame->can_dlc);
            }
            // 不需要等到ACK，直接结束
            if(send_msg->wait_ack_id ==0) break;

            // 等待ACK
            gint64 end_time = g_get_monotonic_time () +  CAN_REPLAY_TIME* G_TIME_SPAN_MILLISECOND;
            g_mutex_lock(g_tl_canbus_data.ack_mutex);
            g_tl_canbus_data.wait_ack_id= send_msg->wait_ack_id;
            if(!g_cond_wait_until(g_tl_canbus_data.ack_cond,g_tl_canbus_data.ack_mutex,end_time)){
                 // time out passed
                 g_mutex_unlock(g_tl_canbus_data.ack_mutex);
                 g_message("wait can ack time out passed");
                 continue;
             }
            g_mutex_unlock(g_tl_canbus_data.ack_mutex);
            g_message("CAN wait ack ok");
            break;
        }
        // 释放CAN发送队列中发送链表中各个can_frame
         g_slist_free_full(send_msg->msg_list, g_free);
         // 释放当前处理的CAN发送队列
         g_free(send_msg);
    }
}

gboolean tl_canbus_init(gboolean use_vcan)
{
    GSList *device_list, *list_foreach;

    if(g_tl_canbus_data.initialized)
    {
        g_warning("TLCANBus already initialized!");
        return TRUE;
    }

    g_tl_canbus_data.socket_table = g_hash_table_new_full(g_str_hash,
    g_str_equal, NULL, (GDestroyNotify)tl_canbus_socket_data_free);

    device_list = tl_canbus_scan_devices(use_vcan);
    if(device_list==NULL)
    {
        g_warning("TLCANBus no CANBus device detected!");
    }


    for(list_foreach=device_list;list_foreach!=NULL;
        list_foreach=g_slist_next(list_foreach))
    {
        tl_canbus_open_socket((const gchar *)list_foreach->data);
    }

    g_tl_canbus_data.data_timestamp = g_get_monotonic_time();
    g_tl_canbus_data.initialized = TRUE;

    g_tl_canbus_data.check_timeout_id = g_timeout_add_seconds(5,
        tl_canbus_check_timeout_cb, &g_tl_canbus_data);

    g_tl_canbus_data.ack_cond = g_new(GCond,1);
    g_cond_init(g_tl_canbus_data.ack_cond);

    g_tl_canbus_data.ack_mutex = g_new(GMutex,1);
    g_mutex_init(g_tl_canbus_data.ack_mutex);

    g_tl_canbus_data.msg_send_queue =g_async_queue_new();

   TLCANBusSocketData *socket_data =  (TLCANBusSocketData *)g_hash_table_lookup(g_tl_canbus_data.socket_table,device_list->data);
   if(socket_data!=NULL)
   {
       g_tl_canbus_data.thread_send_msg = g_thread_new("tl-canbus-thread",
           tl_canbus_thread_send_msg,socket_data);
   }
     g_slist_free_full(device_list, g_free);
    return TRUE;
}

void tl_canbus_uninit()
{
    if(!g_tl_canbus_data.initialized)
    {
        return;
    }

    if(g_tl_canbus_data.check_timeout_id>0)
    {
        g_source_remove(g_tl_canbus_data.check_timeout_id);
        g_tl_canbus_data.check_timeout_id = 0;
    }

    if(g_tl_canbus_data.socket_table!=NULL)
    {
        g_hash_table_unref(g_tl_canbus_data.socket_table);
        g_tl_canbus_data.socket_table = NULL;
    }
    if(g_tl_canbus_data.ack_cond!=NULL)
    {
        g_cond_clear(g_tl_canbus_data.ack_cond);
        g_tl_canbus_data.ack_cond=NULL;

    }
    if(g_tl_canbus_data.ack_mutex!=NULL)
    {
        g_mutex_clear(g_tl_canbus_data.ack_mutex);
        g_tl_canbus_data.ack_mutex=NULL;

    }
    if(g_tl_canbus_data.msg_send_queue!=NULL)
    {
        g_async_queue_unref(g_tl_canbus_data.msg_send_queue);
        g_tl_canbus_data.msg_send_queue =NULL;
    }

    g_tl_canbus_data.initialized = FALSE;
}
/**
 * @brief lock_auth_msg_set 保存车锁授权信息
 * @param data 从远程获取的车锁授权信息
 */
void lock_auth_msg_set(guint8* data)
{
    LockAuthMsg *msg = (LockAuthMsg *)data;
    memcpy(&tl_canbus_lock_auth_msg,msg,sizeof(tl_canbus_lock_auth_msg));
    g_message("LockAuthMsg size: %d, %d", sizeof(LockAuthMsg),sizeof(msg->driver));
}

/**
 * @brief lock_auth_msg_can_send 车锁授权信息下发
 */
void lock_auth_msg_can_send()
{
    LockAuthMsg* msg = &tl_canbus_lock_auth_msg;
    struct can_frame *driver_frame = g_new(struct can_frame,1);
    struct can_frame *lock_start_frame = g_new(struct can_frame,1);
    struct can_frame *lock_end_frame = g_new(struct can_frame,1);
    TLCANBusSendMsg *canbus_send_msgs = g_new(TLCANBusSendMsg,1);
    driver_frame->can_id=DRIVER_ID;
    driver_frame->can_dlc=sizeof(msg->driver);
    memcpy(driver_frame->data,msg->driver,sizeof(msg->driver));

//    g_message("driver_frame: can_id: %x, can_dlc: %d",driver_frame->can_id,driver_frame->can_dlc);
//    print_hex_can(driver_frame->data);
    lock_start_frame->can_id=LOCK_START_ID;
    lock_start_frame->can_dlc=sizeof(msg->lock_start);
    memcpy(lock_start_frame->data,msg->lock_start,sizeof(msg->lock_start));
//    g_message("lock_start_frame: can_id: %x, can_dlc: %d",lock_start_frame->can_id,lock_start_frame->can_dlc);
//    print_hex_can(lock_start_frame->data);

    lock_end_frame->can_id=LOCK_END_ID;
    lock_end_frame->can_dlc=sizeof(msg->lock_end);
    memcpy(lock_end_frame->data,msg->lock_end,sizeof(msg->lock_end));
//    g_message("lock_end_frame: can_id: %x, can_dlc: %d",lock_end_frame->can_id,lock_end_frame->can_dlc);
//    print_hex_can(lock_end_frame->data);

    GSList *can_msg_list = NULL;
    can_msg_list = g_slist_prepend (can_msg_list,driver_frame);
    can_msg_list = g_slist_prepend (can_msg_list,lock_start_frame);
    can_msg_list = g_slist_prepend (can_msg_list,lock_end_frame);
    canbus_send_msgs->msg_list = can_msg_list;
    canbus_send_msgs->wait_ack_id = AUTH_ACK_ID;
    tl_canbus_send_msg_add(canbus_send_msgs);

}
