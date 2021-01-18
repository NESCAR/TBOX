#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <gps.h>
#include <math.h>
#include<string.h>
#include "tl-gps.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>


char *uart1= "/dev/ttyUSB1";
char *uart2= "/dev/ttyUSB2";
char *QGPS = "AT+QGPS=1"; // start gps
// ttyUSB1 is the GPS data
// ttyUSB2 is the AT

//void gps_init()
//{
//    char buff[512];
//    int fd1,fd2;
//    if((fd2 = open(uart2, O_RDWR|O_NOCTTY|O_NDELAY))<0){
//            printf("open %s is failed",uart2);
//        }
//        else{
//            printf("open %s is success\n",uart2);
//            set_opt(fd2, 115200, 8, 'N', 1);
//            write(fd2,QGPS,strlen(QGPS));
//            for(int i=0;i<5;i++)
//            {
//                int n= read(fd2,buff,512);
//                buff[n+1]="\0";
//                if(n>=0) break;
//                sleep(1);
//            }
//            printf("%s",buff);
//            close(fd2);
//    }
//    buff[0]="\0";
//    if((fd1 = open(uart1, O_RDWR|O_NOCTTY|O_NDELAY))<0){
//            printf("open %s is failed",uart1);
//        }
//        else{
//            printf("open %s is success\n",uart1);
//            set_opt(fd1, 115200, 8, 'N', 1);
//            while(1)
//            {
//                int n= read(fd1,buff,512);
//                if(n>0)
//                {
//                    buff[n+1]="\0";
//                    print("%s",buff);
//                }
//            }
//    }


//}


