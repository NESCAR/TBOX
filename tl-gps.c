#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gps.h>
#include <math.h>

#include "tl-gps.h"
#include "tl-jtt808-msg.h"

char *uart1= "/dev/ttyUSB1";
char *uart2= "/dev/ttyUSB2";
char *QGPS = "AT+QGPS=1"; // start gps
// ttyUSB1 is the GPS data
// ttyUSB2 is the AT

typedef struct _TLGPSData
{
    gboolean initialized;
    gboolean work_flag;
    GThread *work_thread;

    guint8 state;
    guint32 latitude;
    guint32 longitude;
    guint16 speed;
}TLGPSData;

static TLGPSData g_tl_gps_data = {0};
/**
 * gps采集线程，需要首先使用gpsd开启端口3333
 */
static gpointer tl_gps_work_thread(gpointer user_data)
{
    TLGPSData *gps_data = (TLGPSData *)user_data;
    struct gps_data_t gdata;
    int rc;
    gboolean open_flag = FALSE;

    if(user_data==NULL)
    {
        return NULL;
    }

    gps_data->work_flag = TRUE;

    while(gps_data->work_flag)
    {
        if(!open_flag && (rc=gps_open("127.0.0.1", "3333", &gdata))==-1)
        {
            g_warning("TLGPS failed to open GPS: %s", gps_errstr(rc));
            g_usleep(2000000UL);
            continue;
        }
        else
        {
            open_flag = TRUE;
            gps_stream(&gdata, WATCH_ENABLE | WATCH_JSON, NULL);
        }

        if(gps_waiting(&gdata, 500000))
        {
            if((rc=gps_read(&gdata,NULL,0))==-1)
            {
                g_warning("TLGPS failed to read GPS data: %s", gps_errstr(rc));
                gps_stream(&gdata, WATCH_DISABLE, NULL);
                gps_close (&gdata);
                open_flag = FALSE;
            }
            else
            {
                g_message("gps_waiting is ok");
                g_message("%d", gdata.fix.status);
                g_message("%d", gdata.fix.mode);
                if( (gdata.fix.mode==MODE_2D ||
                    gdata.fix.mode==MODE_3D) && !isnan(gdata.fix.latitude) &&
                    !isnan(gdata.fix.longitude))
                {
                    if(gdata.fix.latitude < 0)
                    {
                        gps_data->state |= 2;
                    }
                    else
                    {
                        gps_data->state &= ~2;
                    }

                    if(gdata.fix.longitude < 0)
                    {
                        gps_data->state |= 4;
                    }
                    else
                    {
                        gps_data->state &= ~4;
                    }

                    gps_data->latitude = gdata.fix.latitude * 1e6;
                    gps_data->longitude = gdata.fix.longitude * 1e6;

                    g_message("Latitude: %lf, longitude: %lf, speed: %lf, "
                        "timestamp: %lf.", gdata.fix.latitude,
                        gdata.fix.longitude, gdata.fix.speed,
                        gdata.fix.time.tv_sec);
                }
                else
                {
                    gps_data->state &= ~1;
                    g_debug("No GPS data available.");
                }
            }
        }

        g_usleep(500000UL);
    }

    if(open_flag)
    {
        gps_stream(&gdata, WATCH_DISABLE, NULL);
        gps_close (&gdata);
    }

    return NULL;
}
/**
 * gps初始化，建立gps采集线程
 */
gboolean tl_gps_init()
{
    if(g_tl_gps_data.initialized)
    {
        g_warning("TLGPS already initialized!");
        return TRUE;
    }

    g_tl_gps_data.state &= ~1;

    g_tl_gps_data.work_thread = g_thread_new("tl-gps-thread",
        tl_gps_work_thread, &g_tl_gps_data);

    g_tl_gps_data.initialized = TRUE;

    return TRUE;
}

void tl_gps_uninit()
{
    if(!g_tl_gps_data.initialized)
    {
        return;
    }

    if(g_tl_gps_data.work_thread!=NULL)
    {
        g_tl_gps_data.work_flag = FALSE;
        g_thread_join(g_tl_gps_data.work_thread);
        g_tl_gps_data.work_thread = NULL;
    }

    g_tl_gps_data.initialized = FALSE;
}

void tl_gps_state_get(guint8 *state, guint32 *latitude, guint32 *longitude)
{
    if(state!=NULL)
    {
        *state = g_tl_gps_data.state;
    }

    if(latitude!=NULL)
    {
        *latitude = g_tl_gps_data.latitude;
    }

    if(longitude!=NULL)
    {
        *longitude = g_tl_gps_data.longitude;
    }
}
/**
 * 读取位置信息数据
 * @param garray，包含位置信息数据的字节流
 */
void tl_gps_message_get(GByteArray *garray)
{
    LocationMsgUp msg, le_msg;
    msg.alarm = 0;
    msg.status =0;
    msg.latitude  = g_tl_gps_data.latitude;
    msg.longitude = g_tl_gps_data.longitude;
    msg.speed = g_tl_gps_data.speed;
//    g_message("Latitude: %d, longitude: %d, speed: %d, "
//                          "timestamp: %d", msg.latitude,
//                          msg.longitude, msg.speed,
//                          0);

    LocationMsgUpToBE(&msg,&le_msg);
    g_byte_array_append(garray,(guint8 *)&le_msg,sizeof(le_msg));
}

