#include<stdio.h>
#include<glib.h>
#include<string.h>
#include<tl-canbus.h>
#include<tl-gps.h>
#include<tl-net.h>


//#define IP "147.98.47.221"


int main()
{
    setbuf(stdout, NULL);
    printf("--------------------mian test begin--------------");
    static GMainLoop *g_tl_main_loop = NULL;
    static gboolean g_tl_main_cmd_use_vcan = TRUE;
//    gps_init();


    g_tl_main_loop = g_main_loop_new(NULL, FALSE);

    tl_net_init();
//    if(!tl_canbus_init(g_tl_main_cmd_use_vcan))
//    {
//        g_error("Cannot initialize parser!");
//        return 3;
//    }




    g_main_loop_run(g_tl_main_loop);
    g_main_loop_unref(g_tl_main_loop);
    return 0;
}


