#include<stdio.h>
#include<glib.h>
#include<string.h>
#include<tl-canbus.h>
#include<tl-gps.h>
#include<tl-net.h>

int main()
{
    setbuf(stdout, NULL);
    g_message("--------------------mian test begin--------------");
    static GMainLoop *g_tl_main_loop = NULL;
    gboolean g_tl_main_cmd_use_can = TRUE;
//    gps_init();


    g_tl_main_loop = g_main_loop_new(NULL, FALSE);
    tl_gps_init();

    tl_net_init();
    if(!tl_canbus_init(g_tl_main_cmd_use_can))
    {
        g_error("Cannot initialize parser!");
        return 3;
    }





    g_main_loop_run(g_tl_main_loop);
    g_main_loop_unref(g_tl_main_loop);
    return 0;
}


