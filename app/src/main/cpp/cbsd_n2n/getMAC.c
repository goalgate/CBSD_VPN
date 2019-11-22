//
// Created by zbsz on 2019/11/20.
//

#include <android/log.h>
#include <jni.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <net/if.h>
#include "cbsd_n2n.h"

int get_WiFiMac(char * mac, int len_limit)    //返回值是实际写入char * mac的字符个数（不包括'\0'）
{
    struct ifreq ifreq;    //ifreq结构体常用来配置和获取ip地址
    int sock;

    if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("socket");
        return -1;
    }
    strcpy (ifreq.ifr_name, "wlan0");    //Currently, only get eth0

    if (ioctl (sock, SIOCGIFHWADDR, &ifreq) < 0)
    {
        perror ("ioctl");
        return -1;
    }

    return snprintf (mac, len_limit, "%X:%X:%X:%X:%X:%X", (unsigned char) ifreq.ifr_hwaddr.sa_data[0], (unsigned char) ifreq.ifr_hwaddr.sa_data[1], (unsigned char) ifreq.ifr_hwaddr.sa_data[2], (unsigned char) ifreq.ifr_hwaddr.sa_data[3], (unsigned char) ifreq.ifr_hwaddr.sa_data[4], (unsigned char) ifreq.ifr_hwaddr.sa_data[5]);
}


int get_ethMac(char * mac, int len_limit)    //返回值是实际写入char * mac的字符个数（不包括'\0'）
{
    struct ifreq ifreq;    //ifreq结构体常用来配置和获取ip地址
    int sock;

    if ((sock = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("socket");
        return -1;
    }
    strcpy (ifreq.ifr_name, "eth0");    //Currently, only get eth0

    if (ioctl (sock, SIOCGIFHWADDR, &ifreq) < 0)
    {
        perror ("ioctl");
        return -1;
    }

    return snprintf (mac, len_limit, "%X:%X:%X:%X:%X:%X", (unsigned char) ifreq.ifr_hwaddr.sa_data[0], (unsigned char) ifreq.ifr_hwaddr.sa_data[1], (unsigned char) ifreq.ifr_hwaddr.sa_data[2], (unsigned char) ifreq.ifr_hwaddr.sa_data[3], (unsigned char) ifreq.ifr_hwaddr.sa_data[4], (unsigned char) ifreq.ifr_hwaddr.sa_data[5]);
}