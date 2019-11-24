////
//// Created by zbsz on 2019/11/19.
////
//
//
//
//#include <sys/types.h>
//#include <sys/select.h>
//#include <android/log.h>
//#include <sys/socket.h>
//#include <strings.h>
//#include <stdlib.h>
//#include <arpa/inet.h>
//#include <unistd.h>
//#include "ServerConnector.h"
//#include "cbsd_n2n.h"
//
//int ServerConnect(){
//    int sockfd, ret, i, h;
//    struct sockaddr_in servaddr;
//    char str1[4096], str2[4096], buf[BUFSIZE], *str;
//    socklen_t len;
//    fd_set   t_set1;
//    struct timeval  tv;
//
//    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "创建网络连接失败,本线程即将终止---socket error!\n");
//        exit(0);
//    };
//
//    bzero(&servaddr, sizeof(servaddr));
//    servaddr.sin_family = AF_INET;
//    servaddr.sin_port = htons(PORT);
//    if (inet_pton(AF_INET, IPSTR, &servaddr.sin_addr) <= 0 ){
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "创建网络连接失败,本线程即将终止--inet_pton error!\n");
//        exit(0);
//    };
//
//    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "连接到服务器失败,connect error!\n");
//        exit(0);
//    }
//    __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "与远端建立了连接\n");
//
//    memset(str2, 0, 4096);
//    strcat(str2, "jsonData=7D5A16BC8DD62C224A8C599E7E988BB07A19F7B49194715533D08117E6B7DFF61A2A3A902565DAC13FFD643936862601E6847103025C27DBC85977040BAAFD4B58BEE86641153CD8EF55511C47CDF96BFA49DB9E6501FAACC15E206B68035192E64769F84E736A425DE324E44673BE07810AE456092AF923");
//    str=(char *)malloc(128);
//    len = strlen(str2);
//    sprintf(str, "%d", len);
//
//    memset(str1, 0, 4096);
//    strcat(str1, "POST daServer/vpn_jk HTTP/1.1\n");
//    strcat(str1, "Host: http://124.172.232.89:8050\n");
//    strcat(str1, "Content-Type: application/json; charset=UTF-8\n");
//    strcat(str1, "Content-Length: ");
//    strcat(str1, str);
//    strcat(str1, "\n\n");
//    //str2的值为post的数据
//    strcat(str1, str2);
//    strcat(str1, "\r\n\r\n");
//    printf("%s\n",str1);
//
//    ret = write(sockfd,str1,strlen(str1));
//    if (ret < 0) {
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "发送失败！错误代码是");
//        exit(0);
//    }else{
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "消息发送成功，共发送了%d个字节！\n\n", ret);
//    }
//
//    FD_ZERO(&t_set1);
//    FD_SET(sockfd, &t_set1);
//
//    while(1){
//        sleep(2);
//        tv.tv_sec= 0;
//        tv.tv_usec= 0;
//        h= 0;
//        printf("--------------->1");
//        h= select(sockfd +1, &t_set1, NULL, NULL, &tv);
//        printf("--------------->2");
//
//        //if (h == 0) continue;
//        if (h < 0) {
//            close(sockfd);
//            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "在读取数据报文时SELECT检测到异常，该异常导致线程终止！\n");
//            return -1;
//        };
//
//        if (h > 0){
//            memset(buf, 0, 4096);
//            i= read(sockfd, buf, 4095);
//            if (i==0){
//                close(sockfd);
//                __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "读取数据报文时发现远端关闭，该线程终止！\n");
//                return -1;
//            }
//
//            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "%s\n", buf);
//        }
//    }
//    close(sockfd);
//
//
//    return 0;
//}