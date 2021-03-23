#include<stdio.h>
#include<glib.h>
#include<string.h>
#include<tl-canbus.h>
#include<tl-gps.h>
#include<tl-net.h>
#include<tl-parser.h>

int main()
{
    setbuf(stdout, NULL);
    g_message("--------------------mian test begin--------------");
    static GMainLoop *g_tl_main_loop = NULL;
    gboolean g_tl_main_cmd_use_can = TRUE;
    gchar *parse_file_path;
    const gchar *conf_file_path;
    conf_file_path = "/home/root";
    gps_init();

    g_tl_main_loop = g_main_loop_new(NULL, FALSE);
    tl_gps_init();
    tl_net_init();
    tl_parser_init();

    parse_file_path = g_build_filename(conf_file_path, "tboxparse.xml", NULL);
    tl_parser_load_parse_file(parse_file_path);
    g_free(parse_file_path);

    if(!tl_canbus_init(g_tl_main_cmd_use_can))
    {
        g_error("Cannot initialize parser!");
        return 3;
    }

    g_main_loop_run(g_tl_main_loop);
    g_main_loop_unref(g_tl_main_loop);
    return 0;
}


