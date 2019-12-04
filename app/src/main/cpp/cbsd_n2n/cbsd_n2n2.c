//
// Created by switchwang(https://github.com/switch-st) on 2018-04-15.
//
#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "cbsd_n2n.h"


static n2n_edge_status_t status;

static int GetEdgeCmd(JNIEnv *env, jobject jcmd, n2n_edge_cmd_t *cmd);

static int GetEdgeCmd2(JNIEnv *env, jclass obj, n2n_edge_cmd_t *cmd);

static void *EdgeRoutine(void *);

static void ResetEdgeStatus(JNIEnv *env, uint8_t cleanup);

static void InitEdgeStatus(void);

jint getIpAddrPrefixLength(JNIEnv *env, jstring netmask);

jstring getRoute(JNIEnv *env, jstring ipAddr, int prefixLength);

static jobject mVpnService = NULL;

static jobject mParcelFileDescriptor = NULL;

const char *fullkey = "zbvpn.login";

const char jsonDataPreffix[] = "jsonData=";

const char *str_svr;

const char *str_ip;

const char *str_mask;

JNIEXPORT jboolean JNICALL Java_cn_cbsd_vpnx_service_VPNXService_vpnOpen(
        JNIEnv *env,
        jobject this) {

    mVpnService = this;
#ifndef NDEBUG
    __android_log_write(ANDROID_LOG_DEBUG, "edge_jni", "in start");
#endif /* #ifndef NDEBUG */
    ResetEdgeStatus(env, 0 /* not cleanup*/);
    if (GetEdgeCmd2(env, this, &status.cmd) != 0) {
//    if (GetEdgeCmd(env, jcmd, &status.cmd) != 0) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }

    int val = fcntl(status.cmd.vpn_fd, F_GETFL);
    if (val == -1) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }
    if ((val & O_NONBLOCK) == O_NONBLOCK) {
        val &= ~O_NONBLOCK;
        val = fcntl(status.cmd.vpn_fd, F_SETFL, val);
        if (val == -1) {
            ResetEdgeStatus(env, 1 /* cleanup*/);
            return JNI_FALSE;
        }
    }

    if ((*env)->GetJavaVM(env, &status.jvm) != JNI_OK) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }
    status.jobj_service = (*env)->NewGlobalRef(env, this);
    jclass cls = (*env)->FindClass(env, "cn/cbsd/vpnx/model/VPNStatus");
    if (!cls) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }
    status.jcls_status = (*env)->NewGlobalRef(env, cls);
    jclass cls_rs = (*env)->FindClass(env, "cn/cbsd/vpnx/model/VPNStatus$RunningStatus");
    if (!cls_rs) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }
    status.jcls_rs = (*env)->NewGlobalRef(env, cls_rs);

    switch (status.edge_type) {
        case EDGE_TYPE_V1:
            status.start_edge = start_edge_v1;
            status.stop_edge = stop_edge_v1;
            break;
        case EDGE_TYPE_V2:
            status.start_edge = start_edge_v2;
            status.stop_edge = stop_edge_v2;
            break;
        case EDGE_TYPE_V2S:
            status.start_edge = start_edge_v2s;
            status.stop_edge = stop_edge_v2s;
            break;
        default:
            ResetEdgeStatus(env, 1 /* cleanup*/);
            return JNI_FALSE;
    }
    status.report_edge_status = report_edge_status;
    pthread_mutex_init(&status.mutex, NULL);
    int ret = pthread_create(&status.tid, NULL, EdgeRoutine, NULL);
    if (ret != 0) {
        ResetEdgeStatus(env, 1 /* cleanup*/);
        return JNI_FALSE;
    }


    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_cn_cbsd_vpnx_service_VPNXService_vpnClose(
        JNIEnv *env,
        jobject this) {

#ifndef NDEBUG
    __android_log_write(ANDROID_LOG_DEBUG, "edge_jni", "in stop");
#endif /* #ifndef NDEBUG */
    ResetEdgeStatus(env, 0 /* not cleanup*/);
    status.report_edge_status = report_edge_status;
    jclass cls_ParcelFileDescriptor = (*env)->GetObjectClass(env, mParcelFileDescriptor);
    jmethodID close = (*env)->GetMethodID(env, cls_ParcelFileDescriptor, "close", "()V");
    (*env)->CallVoidMethod(env, mParcelFileDescriptor, close);
    if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "JNI抛出的异常！");
    }
    (*env)->DeleteLocalRef(env, cls_ParcelFileDescriptor);
    (*env)->DeleteGlobalRef(env, mParcelFileDescriptor);

}

JNIEXPORT jobject JNICALL Java_cn_cbsd_vpnx_service_VPNXService_getVPNStatus(
        JNIEnv *env,
        jclass this) {
    jclass clrRunningStatus = (*env)->FindClass(env, "cn/cbsd/vpnx/model/VPNStatus$RunningStatus");
    jfieldID id = NULL;
    if (status.tid != -1) {
        if (!pthread_kill(status.tid, 0)) {
            pthread_mutex_lock(&status.mutex);
            switch (status.running_status) {
                case EDGE_STAT_CONNECTING:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "CONNECTING",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
                    break;
                case EDGE_STAT_CONNECTED:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "CONNECTED",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
                    break;
                case EDGE_STAT_SUPERNODE_DISCONNECT:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "NODE_DISCONNECT",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
                    break;
                case EDGE_STAT_DISCONNECT:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "DISCONNECT",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
                    break;
                case EDGE_STAT_FAILED:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "FAILED",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
                    break;
                default:
                    id = (*env)->GetStaticFieldID(env, clrRunningStatus, "DISCONNECT",
                                                  "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
            }
            pthread_mutex_unlock(&status.mutex);
        }
    } else {
        id = (*env)->GetStaticFieldID(env, clrRunningStatus, "DISCONNECT",
                                      "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
    }

    return (*env)->GetStaticObjectField(env, clrRunningStatus, id);


}


static glo_con = NULL;
static glo_callback = NULL;

JNIEXPORT void
Java_cn_cbsd_vpnx_service_VPNXService_VPNX_1login(JNIEnv *env, jobject this, jstring server,
                                                  jstring jsonData, jobject callback) {

    char *key = (char *) malloc(8);
    memcpy(key, fullkey, 8);
    int destSize;
    const char *js = (*env)->GetStringUTFChars(env, jsonData, NULL);
    char *js_Enc = DES_Encrypt(js, strlen(js), key, &destSize);
    char *uploadJson = (char *) malloc(1024);
    strcpy(uploadJson, jsonDataPreffix);
    strcat(uploadJson, js_Enc);
    jstring js_uploadJson = (*env)->NewStringUTF(env, uploadJson);
    glo_callback = (*env)->NewGlobalRef(env, callback);
    jclass cls_ServerConnectTool = (*env)->FindClass(env, "cn/cbsd/vpnx/Tool/ServerConnectTool");
    jmethodID con_SCT = (*env)->GetMethodID(env, cls_ServerConnectTool, "<init>", "()V");
    jmethodID post = (*env)->GetMethodID(env, cls_ServerConnectTool, "post", "()V");
    jfieldID fid_UrlandSuffix = (*env)->GetFieldID(env, cls_ServerConnectTool, "UrlandSuffix",
                                                   "Ljava/lang/String;");
    jfieldID fid_jsonData = (*env)->GetFieldID(env, cls_ServerConnectTool, "jsonData",
                                               "Ljava/lang/String;");
    glo_con = (*env)->NewGlobalRef(env, (*env)->NewObject(env, cls_ServerConnectTool, con_SCT));
    (*env)->SetObjectField(env, glo_con, fid_UrlandSuffix, server);
    (*env)->SetObjectField(env, glo_con, fid_jsonData, js_uploadJson);
    (*env)->CallVoidMethod(env, glo_con, post);


    free(key);
    free(js_Enc);
    free(uploadJson);
    (*env)->ReleaseStringChars(env, jsonData, js);
    (*env)->DeleteLocalRef(env, jsonData);
    (*env)->DeleteLocalRef(env, js_uploadJson);
    (*env)->DeleteLocalRef(env, server);
    (*env)->DeleteLocalRef(env, cls_ServerConnectTool);


}

JNIEXPORT void Java_cn_cbsd_vpnx_service_VPNXService_getResult(JNIEnv *env, jobject this) {
    jclass cls_ServerConnectTool = (*env)->GetObjectClass(env, glo_con);
    jfieldID fid_response = (*env)->GetFieldID(env, cls_ServerConnectTool, "response",
                                               "Ljava/lang/String;");
    jclass cls_callback = (*env)->GetObjectClass(env, glo_callback);
    jmethodID id_onResponse = (*env)->GetMethodID(env, cls_callback, "onResponse",
                                                  "(Ljava/lang/String;)V");
    jmethodID id_onResponseJSON = (*env)->GetMethodID(env, cls_callback, "onResponse",
                                                      "(Lorg/json/JSONObject;)V");
    jstring str_response = (jstring) (*env)->GetObjectField(env, glo_con, fid_response);
    if (str_response == NULL) {
        (*env)->CallVoidMethod(env, glo_callback, id_onResponse, str_response);
        (*env)->DeleteLocalRef(env, str_response);
        (*env)->DeleteGlobalRef(env, glo_callback);
        (*env)->DeleteGlobalRef(env, glo_con);
        return;
    }

    char *key = (char *) malloc(8);
    memcpy(key, fullkey, 8);
    const char *response = (*env)->GetStringUTFChars(env, str_response, NULL);
    int destSize;
    char *response_dec = DES_Decrypt(response, strlen(response), key, &destSize);
    if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
        (*env)->CallVoidMethod(env, glo_callback, id_onResponse, (*env)->NewStringUTF(env,"DES解析错误"));
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "DES解析异常！");
        (*env)->DeleteLocalRef(env, str_response);
        (*env)->DeleteGlobalRef(env, glo_callback);
        (*env)->DeleteGlobalRef(env, glo_con);
        return ;
    }
    jclass cls_JSONObject = (*env)->FindClass(env, "org/json/JSONObject");
    jmethodID con_JSONObjectWithString = (*env)->GetMethodID(env, cls_JSONObject, "<init>",
                                                             "(Ljava/lang/String;)V");
    jmethodID getString = (*env)->GetMethodID(env, cls_JSONObject, "getString",
                                              "(Ljava/lang/String;)Ljava/lang/String;");
//    __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "response_dec = %s", response_dec);
    jstring jsonData = (*env)->NewStringUTF(env, response_dec);
    jobject jsonObject = (*env)->NewObject(env, cls_JSONObject, con_JSONObjectWithString, jsonData);

    jstring head_result = (*env)->NewStringUTF(env, "result");
    jstring value_result = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString,
                                                              head_result);
    const char *result_str = (*env)->GetStringUTFChars(env, value_result, NULL);
    if (strcmp(result_str, "true") != 0) {
        (*env)->CallVoidMethod(env, glo_callback, id_onResponse, value_result);
    } else {
        jstring head_svr = (*env)->NewStringUTF(env, "svr");
        jstring value_svr = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString,
                                                               head_svr);
        str_svr = (*env)->GetStringUTFChars(env, value_svr, NULL);
        (*env)->DeleteLocalRef(env, head_svr);
        (*env)->DeleteLocalRef(env, value_svr);

        jstring head_ip = (*env)->NewStringUTF(env, "ip");
        jstring value_ip = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString, head_ip);
        str_ip = (*env)->GetStringUTFChars(env, value_ip, NULL);
        (*env)->DeleteLocalRef(env, head_ip);
        (*env)->DeleteLocalRef(env, value_ip);

        jstring head_mask = (*env)->NewStringUTF(env, "mask");
        jstring value_mask = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString,
                                                                head_mask);
        str_mask = (*env)->GetStringUTFChars(env, value_mask, NULL);
        (*env)->DeleteLocalRef(env, value_mask);
        (*env)->DeleteLocalRef(env, head_mask);

        jstring head_username = (*env)->NewStringUTF(env, "username");
        jstring value_username = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString,
                                                                    head_username);
        jstring head_password = (*env)->NewStringUTF(env, "password");
        jstring value_password = (jstring) (*env)->CallObjectMethod(env, jsonObject, getString,
                                                                    head_password);
        jmethodID con_JSONObject = (*env)->GetMethodID(env, cls_JSONObject, "<init>",
                                                       "()V");
        jmethodID id_put = (*env)->GetMethodID(env, cls_JSONObject, "put",
                                               "(Ljava/lang/String;Ljava/lang/Object;)Lorg/json/JSONObject;");
        jobject jsonBack = (*env)->NewObject(env, cls_JSONObject, con_JSONObject);
        jsonBack = (*env)->CallObjectMethod(env, jsonBack, id_put, head_username, value_username);
        jsonBack = (*env)->CallObjectMethod(env, jsonBack, id_put, head_password, value_password);
        (*env)->CallVoidMethod(env, glo_callback, id_onResponse, value_result);
        (*env)->CallVoidMethod(env, glo_callback, id_onResponseJSON, jsonBack);
        (*env)->DeleteLocalRef(env, head_username);
        (*env)->DeleteLocalRef(env, value_username);
        (*env)->DeleteLocalRef(env, head_password);
        (*env)->DeleteLocalRef(env, value_password);
        (*env)->DeleteLocalRef(env, jsonBack);
        if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
            (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "JNI抛出的异常！");
        }
    }

    (*env)->DeleteLocalRef(env, head_result);
    (*env)->DeleteLocalRef(env, jsonData);
    (*env)->DeleteLocalRef(env, jsonObject);
    (*env)->ReleaseStringChars(env, value_result, result_str);
    (*env)->DeleteLocalRef(env, cls_callback);
    (*env)->DeleteLocalRef(env, cls_JSONObject);
    (*env)->DeleteLocalRef(env, cls_ServerConnectTool);

    free(key);
    free(response_dec);
    (*env)->ReleaseStringChars(env, str_response, response);
    (*env)->DeleteGlobalRef(env, glo_callback);
    (*env)->DeleteGlobalRef(env, glo_con);

}

/////////////////////////////////////////////////////////////////////////
#ifndef JNI_CHECKNULL
#define JNI_CHECKNULL(p)            do { if (!(p)) return 1;}while(0)
#endif /* JNI_CHECKNULL */

int GetEdgeCmd(JNIEnv *env, jobject jcmd, n2n_edge_cmd_t *cmd) {
    jclass cls;
    int i, j;

    cls = (*env)->GetObjectClass(env, jcmd);
    JNI_CHECKNULL(cls);

    // edgeType
    {
        jint jiEdgeType = (*env)->GetIntField(env, jcmd,
                                              (*env)->GetFieldID(env, cls, "edgeType", "I"));
        if (jiEdgeType < EDGE_TYPE_V1 || jiEdgeType > EDGE_TYPE_V2S) {
            return 1;
        }
        status.edge_type = jiEdgeType;

#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "edgeType = %d", status.edge_type);
#endif /* #ifndef NDEBUG */
    }

    // ipAddr
    {
        jstring jsIpAddr = (*env)->GetObjectField(env, jcmd, (*env)->GetFieldID(env, cls, "ipAddr",
                                                                                "Ljava/lang/String;"));
        JNI_CHECKNULL(jsIpAddr);
        const char *ipAddr = (*env)->GetStringUTFChars(env, jsIpAddr, NULL);
        if (!ipAddr || strlen(ipAddr) == 0) {
            (*env)->ReleaseStringUTFChars(env, jsIpAddr, ipAddr);
            return 1;
        }
        strncpy(cmd->ip_addr, ipAddr, EDGE_CMD_IPSTR_SIZE);
        (*env)->ReleaseStringUTFChars(env, jsIpAddr, ipAddr);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "ipAddr = %s", cmd->ip_addr);
#endif /* #ifndef NDEBUG */
    }
    // ipNetmask
    {
        jstring jsIpNetmask = (*env)->GetObjectField(env, jcmd,
                                                     (*env)->GetFieldID(env, cls, "ipNetmask",
                                                                        "Ljava/lang/String;"));
        JNI_CHECKNULL(jsIpNetmask);
        const char *ipNetmask = (*env)->GetStringUTFChars(env, jsIpNetmask, NULL);
        if (!ipNetmask || strlen(ipNetmask) == 0) {
            (*env)->ReleaseStringUTFChars(env, jsIpNetmask, ipNetmask);
            return 1;
        }
        strncpy(cmd->ip_netmask, ipNetmask, EDGE_CMD_IPSTR_SIZE);
        (*env)->ReleaseStringUTFChars(env, jsIpNetmask, ipNetmask);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "ipNetmask = %s", cmd->ip_netmask);
#endif /* #ifndef NDEBUG */
    }
    // supernodes
    {
        jarray jaSupernodes = (*env)->GetObjectField(env, jcmd,
                                                     (*env)->GetFieldID(env, cls, "supernodes",
                                                                        "[Ljava/lang/String;"));
        JNI_CHECKNULL(jaSupernodes);
        int len = (*env)->GetArrayLength(env, jaSupernodes);
        if (len <= 0) {
            return 1;
        }
        for (i = 0, j = 0; i < len && i < EDGE_CMD_SUPERNODES_NUM; ++i) {
            const jobject jsNode = (*env)->GetObjectArrayElement(env, jaSupernodes, i);
            if (!jsNode) {
                continue;
            }
            const char *node = (*env)->GetStringUTFChars(env, jsNode, NULL);
            if (!node || strlen(node) == 0) {
                (*env)->ReleaseStringUTFChars(env, jsNode, node);
                continue;
            }
            strncpy(cmd->supernodes[j], node, EDGE_CMD_SN_HOST_SIZE);
            (*env)->ReleaseStringUTFChars(env, jsNode, node);
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "supernodes = %s",
                                cmd->supernodes[j]);
#endif /* #ifndef NDEBUG */
            j++;
        }
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "j = %d", j);
#endif /* #ifndef NDEBUG */
        if (j == 0) {
            return 1;
        }
    }
    // community
    {
        jstring jsCommunity = (*env)->GetObjectField(env, jcmd,
                                                     (*env)->GetFieldID(env, cls, "community",
                                                                        "Ljava/lang/String;"));
        JNI_CHECKNULL(jsCommunity);
        const char *community = (*env)->GetStringUTFChars(env, jsCommunity, NULL);
        if (!community || strlen(community) == 0) {
            (*env)->ReleaseStringUTFChars(env, jsCommunity, community);
            return 1;
        }
        strncpy(cmd->community, community, EDGE_CMD_COMMUNITY_SIZE);
        (*env)->ReleaseStringUTFChars(env, jsCommunity, community);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "community = %s", cmd->community);
#endif /* #ifndef NDEBUG */
    }
    // encKey
    {
        jstring jsEncKey = (*env)->GetObjectField(env, jcmd, (*env)->GetFieldID(env, cls, "encKey",
                                                                                "Ljava/lang/String;"));
        if (jsEncKey) {
            const char *encKey = (*env)->GetStringUTFChars(env, jsEncKey, NULL);
            if (encKey && strlen(encKey) != 0) {
                cmd->enc_key = strdup(encKey);
            }
            (*env)->ReleaseStringUTFChars(env, jsEncKey, encKey);
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "encKey = %s", cmd->enc_key);
#endif /* #ifndef NDEBUG */
        }
    }
    // encKeyFile
    if (status.edge_type == EDGE_TYPE_V2 || status.edge_type == EDGE_TYPE_V2S) {
        jstring jsEncKeyFile = (*env)->GetObjectField(env, jcmd,
                                                      (*env)->GetFieldID(env, cls, "encKeyFile",
                                                                         "Ljava/lang/String;"));
        if (jsEncKeyFile) {
            const char *encKeyFile = (*env)->GetStringUTFChars(env, jsEncKeyFile, NULL);
            if (encKeyFile && strlen(encKeyFile) != 0) {
                cmd->enc_key_file = strdup(encKeyFile);
            }
            (*env)->ReleaseStringUTFChars(env, jsEncKeyFile, encKeyFile);
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "encKeyFile = %s",
                                cmd->enc_key_file);
#endif /* #ifndef NDEBUG */
        }
    }
    // macAddr
    {
        jstring jsMacAddr = (*env)->GetObjectField(env, jcmd,
                                                   (*env)->GetFieldID(env, cls, "macAddr",
                                                                      "Ljava/lang/String;"));
        JNI_CHECKNULL(jsMacAddr);
        const char *macAddr = (*env)->GetStringUTFChars(env, jsMacAddr, NULL);
        if (macAddr && strlen(macAddr) != 0) {
            strncpy(cmd->mac_addr, macAddr, EDGE_CMD_MACNAMSIZ);
        }
        (*env)->ReleaseStringUTFChars(env, jsMacAddr, macAddr);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "macAddr = %s", cmd->mac_addr);
#endif /* #ifndef NDEBUG */
    }
    // mtu
    {
        jint jiMtu = (*env)->GetIntField(env, jcmd, (*env)->GetFieldID(env, cls, "mtu", "I"));
        if (jiMtu <= 0) {
            return 1;
        }
        cmd->mtu = jiMtu;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "mtu = %d", cmd->mtu);
#endif /* #ifndef NDEBUG */
    }
    // localIP
    if (status.edge_type == EDGE_TYPE_V2S) {
        jstring jsLocalIP = (*env)->GetObjectField(env, jcmd,
                                                   (*env)->GetFieldID(env, cls, "localIP",
                                                                      "Ljava/lang/String;"));
        JNI_CHECKNULL(jsLocalIP);
        const char *localIP = (*env)->GetStringUTFChars(env, jsLocalIP, NULL);
        if (localIP && strlen(localIP) != 0) {
            strncpy(cmd->local_ip, localIP, EDGE_CMD_IPSTR_SIZE);
        }
        (*env)->ReleaseStringUTFChars(env, jsLocalIP, localIP);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "localIP = %s", cmd->local_ip);
#endif /* #ifndef NDEBUG */
    }
    // holePunchInterval
    if (status.edge_type == EDGE_TYPE_V2S) {
        jint jiHolePunchInterval = (*env)->GetIntField(env, jcmd, (*env)->GetFieldID(env, cls,
                                                                                     "holePunchInterval",
                                                                                     "I"));
        if (jiHolePunchInterval <= 0) {
            return 1;
        }
        cmd->holepunch_interval = jiHolePunchInterval;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "holePunchInterval = %d",
                            cmd->holepunch_interval);
#endif /* #ifndef NDEBUG */
    }
    // reResoveSupernodeIP
    {
        jboolean jbReResoveSupernodeIP = (*env)->GetBooleanField(env, jcmd,
                                                                 (*env)->GetFieldID(env, cls,
                                                                                    "reResoveSupernodeIP",
                                                                                    "Z"));
        cmd->re_resolve_supernode_ip = jbReResoveSupernodeIP ? 1 : 0;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "reResoveSupernodeIP = %d",
                            cmd->re_resolve_supernode_ip);
#endif /* #ifndef NDEBUG */
    }
    // localPort
    {
        jint jiLocalPort = (*env)->GetIntField(env, jcmd,
                                               (*env)->GetFieldID(env, cls, "localPort", "I"));
        if (jiLocalPort < 0) {
            return 1;
        }
        cmd->local_port = jiLocalPort;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "localPort = %d", cmd->local_port);
#endif /* #ifndef NDEBUG */
    }
    // allowRouting
    {
        jboolean jbAllowRouting = (*env)->GetBooleanField(env, jcmd, (*env)->GetFieldID(env, cls,
                                                                                        "allowRouting",
                                                                                        "Z"));
        cmd->allow_routing = jbAllowRouting ? 1 : 0;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "allowRouting = %d", cmd->allow_routing);
#endif /* #ifndef NDEBUG */
    }
    // dropMuticast
    if (status.edge_type == EDGE_TYPE_V2 || status.edge_type == EDGE_TYPE_V2S) {
        jboolean jbDropMuticast = (*env)->GetBooleanField(env, jcmd, (*env)->GetFieldID(env, cls,
                                                                                        "dropMuticast",
                                                                                        "Z"));
        cmd->drop_multicast = jbDropMuticast ? 1 : 0;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "dropMuticast = %d",
                            cmd->drop_multicast);
#endif /* #ifndef NDEBUG */
    }
    // httpTunnel
    if (status.edge_type == EDGE_TYPE_V1) {
        jboolean jbHttpTunnel = (*env)->GetBooleanField(env, jcmd,
                                                        (*env)->GetFieldID(env, cls, "httpTunnel",
                                                                           "Z"));
        cmd->http_tunnel = jbHttpTunnel ? 1 : 0;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "httpTunnel = %d", cmd->http_tunnel);
#endif /* #ifndef NDEBUG */
    }
    // traceLevel
    {
        jint jiTraceLevel = (*env)->GetIntField(env, jcmd,
                                                (*env)->GetFieldID(env, cls, "traceLevel", "I"));
        cmd->trace_vlevel = jiTraceLevel;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "traceLevel = %d", cmd->trace_vlevel);
#endif /* #ifndef NDEBUG */
    }
    // vpnFd
    {
        jint jiVpnFd = (*env)->GetIntField(env, jcmd, (*env)->GetFieldID(env, cls, "vpnFd", "I"));
        if (jiVpnFd < 0) {
            return 1;
        }
        cmd->vpn_fd = jiVpnFd;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "vpnFd = %d", cmd->vpn_fd);
#endif /* #ifndef NDEBUG */
    }
    // logPath
    jstring jsLogPath = (*env)->GetObjectField(env, jcmd, (*env)->GetFieldID(env, cls, "logPath",
                                                                             "Ljava/lang/String;"));
    JNI_CHECKNULL(jsLogPath);
    const char *logPath = (*env)->GetStringUTFChars(env, jsLogPath, NULL);
    if (logPath && strlen(logPath) != 0) {
        cmd->logpath = strdup(logPath);
    }
    (*env)->ReleaseStringUTFChars(env, jsLogPath, logPath);
#ifndef NDEBUG
    __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "logPath = %s", cmd->logpath);
#endif /* #ifndef NDEBUG */
    return 0;
}

int GetEdgeCmd2(JNIEnv *env, jclass obj, n2n_edge_cmd_t *cmd) {

    // edgeType
    {

        status.edge_type = 0;

#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "edgeType = %d", status.edge_type);
#endif /* #ifndef NDEBUG */
    }

    // ipAddr
    {
//        const char *ipAddr = "10.0.101.67";
        strncpy(cmd->ip_addr, str_ip, EDGE_CMD_IPSTR_SIZE);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "ipAddr = %s", cmd->ip_addr);
#endif /* #ifndef NDEBUG */
    }
    // ipNetmask
    {

//        const char *ipNetmask = "255.0.0.0";
        if (!str_mask || strlen(str_mask) == 0) {
            return 1;
        }
        strncpy(cmd->ip_netmask, str_mask, EDGE_CMD_IPSTR_SIZE);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "ipNetmask = %s", cmd->ip_netmask);
#endif /* #ifndef NDEBUG */
    }
    // supernodes
    {

//        const char *node = "203.195.140.102:5465";
        strncpy(cmd->supernodes[0], str_svr, EDGE_CMD_SN_HOST_SIZE);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "supernodes = %s",
                            cmd->supernodes[0]);
#endif /* #ifndef NDEBUG */

    }
    // community
    {

        const char *community = "sxsp";
        strncpy(cmd->community, community, EDGE_CMD_COMMUNITY_SIZE);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "community = %s", cmd->community);
#endif /* #ifndef NDEBUG */
    }
    // encKey
    {
        const char *encKey = "sxsp@cbsd";
        if (encKey && strlen(encKey) != 0) {
            cmd->enc_key = strdup(encKey);
        }
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "encKey = %s", cmd->enc_key);
#endif /* #ifndef NDEBUG */

    }

    // macAddr
    {
        char szMac[18];
        if (get_WiFiMac(szMac, sizeof(szMac)) > 0) {
            strncpy(status.cmd.mac_addr, szMac, EDGE_CMD_MACNAMSIZ);
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "Mac = %s", szMac);
#endif
        } else if (get_ethMac(szMac, sizeof(szMac)) > 0) {
            strncpy(status.cmd.mac_addr, szMac, EDGE_CMD_MACNAMSIZ);
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "Mac = %s", szMac);
#endif
        } else {
#ifndef NDEBUG
            __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "Mac = NULL");
#endif
        }
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "macAddr = %s", cmd->mac_addr);
#endif /* #ifndef NDEBUG */
    }

    // mtu
    {
        cmd->mtu = 1400;
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "mtu = %d", cmd->mtu);
#endif /* #ifndef NDEBUG */
    }

    // vpnFd
    {
//        jmethodID construct_VpnService = (*env)->GetMethodID(env, Global_cls, "<init>", "()V");
//        jobject mVpnService = (*env)->NewObject(env, Global_cls, construct_VpnService);

        jclass cls_Builder = (*env)->FindClass(env, "android/net/VpnService$Builder");
        jmethodID construct_Builder = (*env)->GetMethodID(env, cls_Builder, "<init>",
                                                          "(Landroid/net/VpnService;)V");

        jobject mBuilder = (*env)->NewObject(env, cls_Builder, construct_Builder, mVpnService);
        jmethodID setMtu = (*env)->GetMethodID(env, cls_Builder, "setMtu",
                                               "(I)Landroid/net/VpnService$Builder;");
        jmethodID addAddress = (*env)->GetMethodID(env, cls_Builder, "addAddress",
                                                   "(Ljava/lang/String;I)Landroid/net/VpnService$Builder;");
        jmethodID addRoute = (*env)->GetMethodID(env, cls_Builder, "addRoute",
                                                 "(Ljava/lang/String;I)Landroid/net/VpnService$Builder;");
        jmethodID setSession = (*env)->GetMethodID(env, cls_Builder, "setSession",
                                                   "(Ljava/lang/String;)Landroid/net/VpnService$Builder;");
        jmethodID establish = (*env)->GetMethodID(env, cls_Builder, "establish",
                                                  "()Landroid/os/ParcelFileDescriptor;");

//        jclass cls_NetAddressTool = (*env)->FindClass(env, "cn/cbsd/vpnx/Tool/NetAddressTool");
//        jmethodID id_getIpAddrPrefixLength = (*env)->GetStaticMethodID(env, cls_NetAddressTool,
//                                                                       "getIpAddrPrefixLength",
//                                                                       "(Ljava/lang/String;)I");
//
//        jmethodID id_getRoute = (*env)->GetStaticMethodID(env, cls_NetAddressTool, "getRoute",
//                                                          "(Ljava/lang/String;I)Ljava/lang/String;");


        jclass cls_ParcelFileDescriptor = (*env)->FindClass(env, "android/os/ParcelFileDescriptor");
        jmethodID detachFd = (*env)->GetMethodID(env, cls_ParcelFileDescriptor, "detachFd", "()I");

        jstring netmask = (*env)->NewStringUTF(env, &status.cmd.ip_netmask);
        jstring ipaddr = (*env)->NewStringUTF(env, &status.cmd.ip_addr);
        jstring sessionName = (*env)->NewStringUTF(env, "vpn_v1");

//        jint prefixLength = (*env)->CallStaticIntMethod(env, cls_NetAddressTool,
//                                                        id_getIpAddrPrefixLength, netmask);
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", ". = %d",
//                            prefixLength);
//
//
//        jstring Route = (jstring) (*env)->CallStaticObjectMethod(env, cls_NetAddressTool,
//                                                                 id_getRoute, ipaddr, prefixLength);
//        const char *sd = (*env)->GetStringUTFChars(env, Route, NULL);
//        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "sd1 = %s",
//                            sd);
//        const char *sd2 = (*env)->GetStringUTFChars(env, Route, NULL);

        jint prefixLength = getIpAddrPrefixLength(env, netmask);

        jstring Route = getRoute(env, ipaddr, prefixLength);


        (*env)->CallObjectMethod(env, mBuilder, setMtu, 1400);
        (*env)->CallObjectMethod(env, mBuilder, addAddress, ipaddr, prefixLength);
        (*env)->CallObjectMethod(env, mBuilder, addRoute, Route, prefixLength);
        (*env)->CallObjectMethod(env, mBuilder, setSession, sessionName);

        mParcelFileDescriptor = (*env)->NewGlobalRef(env, (*env)->CallObjectMethod(env, mBuilder,
                                                                                   establish));
        if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
            (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "JNI抛出的异常！");
            return 1;
        }
        cmd->vpn_fd = (*env)->CallIntMethod(env, mParcelFileDescriptor, detachFd);
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni", "vpnFd = %d", cmd->vpn_fd);
#endif /* #ifndef NDEBUG */
//
        (*env)->DeleteLocalRef(env, cls_Builder);
        (*env)->DeleteLocalRef(env, cls_ParcelFileDescriptor);
        (*env)->DeleteLocalRef(env, sessionName);
        (*env)->DeleteLocalRef(env, mBuilder);
        (*env)->DeleteLocalRef(env, netmask);
        (*env)->DeleteLocalRef(env, ipaddr);
        (*env)->DeleteLocalRef(env, Route);
    }
    return 0;
}


void InitEdgeStatus(void) {
    memset(&status.cmd, 0, sizeof(status.cmd));
    status.cmd.enc_key = NULL;
    status.cmd.enc_key_file = NULL;
    status.cmd.mtu = 1400;
    status.cmd.holepunch_interval = EDGE_CMD_HOLEPUNCH_INTERVAL;
    status.cmd.re_resolve_supernode_ip = 0;
    status.cmd.local_port = 0;
    status.cmd.allow_routing = 0;
    status.cmd.drop_multicast = 1;
    status.cmd.http_tunnel = 0;
    status.cmd.trace_vlevel = 1;
    status.cmd.vpn_fd = -1;
    status.cmd.logpath = NULL;

    status.tid = -1;
    status.jvm = NULL;
    status.jobj_service = NULL;
    status.jcls_status = NULL;
    status.jcls_rs = NULL;
    status.start_edge = NULL;
    status.stop_edge = NULL;
    status.report_edge_status = NULL;

    status.edge_type = EDGE_TYPE_NONE;
    status.running_status = EDGE_STAT_DISCONNECT;
}

void ResetEdgeStatus(JNIEnv *env, uint8_t cleanup) {
    static u_int8_t once = 0;
    static pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

    if (!once) {
        InitEdgeStatus();
        once = 1;
        return;
    }

    pthread_mutex_lock(&mut);
    if (status.tid != -1 || cleanup) {
#ifndef NDEBUG
        __android_log_print(ANDROID_LOG_DEBUG, "edge_jni",
                            "ResetEdgeStatus tid = %ld, cleanup = %d", status.tid, cleanup);
#endif /* #ifndef NDEBUG */
        if (status.stop_edge) {
            status.stop_edge();
        }
        if (status.tid != -1) {
            pthread_join(status.tid, NULL);
        }
        pthread_mutex_lock(&status.mutex);
        if (env) {
            if (status.jcls_rs) {
                (*env)->DeleteGlobalRef(env, status.jcls_rs);
            }
            if (status.jcls_status) {
                (*env)->DeleteGlobalRef(env, status.jcls_status);
            }
            if (status.jobj_service) {
                (*env)->DeleteGlobalRef(env, status.jobj_service);
            }
        }
        if (status.cmd.enc_key_file) {
            free(status.cmd.enc_key_file);
        }
        if (status.cmd.enc_key) {
            free(status.cmd.enc_key);
        }
        if (status.cmd.logpath) {
            free(status.cmd.logpath);
        }
        InitEdgeStatus();
        pthread_mutex_unlock(&status.mutex);
        pthread_mutex_destroy(&status.mutex);
    }
    pthread_mutex_unlock(&mut);
}

void *EdgeRoutine(void *ignore) {
    int flag = 0;
    JNIEnv *env = NULL;

    if (status.jvm) {
        if ((*status.jvm)->AttachCurrentThread(status.jvm, &env, NULL) == JNI_OK) {
            flag = 1;
        }
    }

    if (!status.start_edge) {
        return (void *) -1;
    }

    int ret = status.start_edge(&status);
    if (ret) {
        pthread_mutex_lock(&status.mutex);
        status.running_status = EDGE_STAT_FAILED;
        pthread_mutex_unlock(&status.mutex);
        report_edge_status();
    }

    if (flag && status.jvm) {
        (*status.jvm)->DetachCurrentThread(status.jvm);
    }

    return (void *) ret;
}

void report_edge_status(void) {
    if (!status.jvm || !status.jobj_service || !status.jcls_status || !status.jcls_rs ||
        status.tid == -1) {
        return;
    }

    const char *running_status = "DISCONNECT";
    pthread_mutex_lock(&status.mutex);
    switch (status.running_status) {
        case EDGE_STAT_CONNECTING:
            running_status = "CONNECTING";
            break;
        case EDGE_STAT_CONNECTED:
            running_status = "CONNECTED";
            break;
        case EDGE_STAT_SUPERNODE_DISCONNECT:
            running_status = "NODE_DISCONNECT";
            break;
        case EDGE_STAT_DISCONNECT:
            running_status = "DISCONNECT";
            break;
        case EDGE_STAT_FAILED:
            running_status = "FAILED";
            break;
        default:
            running_status = "DISCONNECT";
    }
    pthread_mutex_unlock(&status.mutex);

    JNIEnv *env = NULL;
    if ((*status.jvm)->GetEnv(status.jvm, &env, JNI_VERSION_1_1) != JNI_OK || !env) {
        return;
    }

    jmethodID mid = (*env)->GetMethodID(env, status.jcls_status, "<init>", "()V");
    if (!mid) {
        return;
    }
    jobject jStatus = (*env)->NewObject(env, status.jcls_status, mid);
    if (!jStatus) {
        return;
    }

    jfieldID fid = (*env)->GetStaticFieldID(env, status.jcls_rs, running_status,
                                            "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
    if (!fid) {
        (*env)->DeleteLocalRef(env, jStatus);
        return;
    }
    jobject jRunningStatus = (*env)->GetStaticObjectField(env, status.jcls_rs, fid);
    if (!jRunningStatus) {
        (*env)->DeleteLocalRef(env, jRunningStatus);
        (*env)->DeleteLocalRef(env, jStatus);
        return;
    }
    fid = (*env)->GetFieldID(env, status.jcls_status, "runningStatus",
                             "Lcn/cbsd/vpnx/model/VPNStatus$RunningStatus;");
    if (!fid) {
        (*env)->DeleteLocalRef(env, jRunningStatus);
        (*env)->DeleteLocalRef(env, jStatus);
        return;
    }
    (*env)->SetObjectField(env, jStatus, fid, jRunningStatus);


    jclass cls = (*env)->GetObjectClass(env, status.jobj_service);
    if (!cls) {
        (*env)->DeleteLocalRef(env, jRunningStatus);
        (*env)->DeleteLocalRef(env, jStatus);
        return;
    }
    mid = (*env)->GetMethodID(env, cls, "reportVPNStatus",
                              "(Lcn/cbsd/vpnx/model/VPNStatus;)V");
    if (!mid) {
        (*env)->DeleteLocalRef(env, jRunningStatus);
        (*env)->DeleteLocalRef(env, jStatus);
        return;
    }
    (*env)->CallVoidMethod(env, status.jobj_service, mid, jStatus);

    (*env)->DeleteLocalRef(env, jRunningStatus);
    (*env)->DeleteLocalRef(env, jStatus);
}


jint getIpAddrPrefixLength(JNIEnv *env, jstring netmask) {
    jclass cls_InetAddress = (*env)->FindClass(env, "java/net/InetAddress");
    jmethodID getByName = (*env)->GetStaticMethodID(env, cls_InetAddress, "getByName",
                                                    "(Ljava/lang/String;)Ljava/net/InetAddress;");
    jmethodID getAddress = (*env)->GetMethodID(env, cls_InetAddress, "getAddress", "()[B");
    jobject mInetAddress = (*env)->CallStaticObjectMethod(env, cls_InetAddress, getByName, netmask);
    jbyteArray byteAddr = (jbyteArray) (*env)->CallObjectMethod(env, mInetAddress, getAddress);

    int prefixLength = 0;

    int byteAddrLen = (*env)->GetArrayLength(env, byteAddr);
    jbyte *jbarray = (*env)->GetByteArrayElements(env, byteAddr, NULL);

    for (int i = 0; i < byteAddrLen; i++) {
        for (int j = 0; j < 8; j++) {
            if ((jbarray[i] << j & 0xFF) != 0) {
                prefixLength++;
            } else {
                (*env)->DeleteLocalRef(env, mInetAddress);
                (*env)->ReleaseByteArrayElements(env, byteAddr, jbarray, 0);
                (*env)->DeleteLocalRef(env, byteAddr);
                (*env)->DeleteLocalRef(env, cls_InetAddress);
                return prefixLength;
            }
        }
    }
    (*env)->DeleteLocalRef(env, mInetAddress);
    (*env)->DeleteLocalRef(env, byteAddr);
    (*env)->DeleteLocalRef(env, jbarray);
    (*env)->DeleteLocalRef(env, cls_InetAddress);

    if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "JNI抛出的异常！");
        return -1;
    }
    return prefixLength;
}


jstring getRoute(JNIEnv *env, jstring ipAddr, int prefixLength) {
    jbyte arr[] = {0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};
    if (prefixLength > 32 || prefixLength < 0) {
        return NULL;
    }
    jclass cls_InetAddress = (*env)->FindClass(env, "java/net/InetAddress");
    jmethodID getByName = (*env)->GetStaticMethodID(env, cls_InetAddress, "getByName",
                                                    "(Ljava/lang/String;)Ljava/net/InetAddress;");
    jmethodID getAddress = (*env)->GetMethodID(env, cls_InetAddress, "getAddress", "()[B");

    jobject mInetAddress = (*env)->CallStaticObjectMethod(env, cls_InetAddress, getByName, ipAddr);

    jbyteArray byteAddr = (jbyteArray) (*env)->CallObjectMethod(env, mInetAddress, getAddress);
    int byteAddrLen = (*env)->GetArrayLength(env, byteAddr);
    jbyte *jbarray = (*env)->GetByteArrayElements(env, byteAddr, JNI_FALSE);
    int idx = 0;
    while (prefixLength >= 8) {
        idx++;
        prefixLength -= 8;
    }
    if (idx < byteAddrLen) {
        jbarray[idx++] &= arr[prefixLength];
    }
    for (; idx < byteAddrLen; idx++) {
        jbarray[idx] = (jbyte) 0x00;
    }

    jbyteArray jbyteArrayBack = (*env)->NewByteArray(env, byteAddrLen);
    (*env)->SetByteArrayRegion(env, jbyteArrayBack, 0, byteAddrLen, jbarray);

    jmethodID getByAddress = (*env)->GetStaticMethodID(env, cls_InetAddress, "getByAddress",
                                                       "([B)Ljava/net/InetAddress;");
    jmethodID getHostAddress = (*env)->GetMethodID(env, cls_InetAddress, "getHostAddress",
                                                   "()Ljava/lang/String;");

    jobject InetAddressBack = (*env)->CallStaticObjectMethod(env, cls_InetAddress, getByAddress,
                                                             jbyteArrayBack);
    jstring HostAddress = (jstring) (*env)->CallObjectMethod(env, InetAddressBack, getHostAddress);

    (*env)->DeleteLocalRef(env, cls_InetAddress);
    (*env)->DeleteLocalRef(env, mInetAddress);
    (*env)->DeleteLocalRef(env, jbyteArrayBack);
    (*env)->ReleaseByteArrayElements(env, byteAddr, jbarray, 0);
    (*env)->DeleteLocalRef(env, byteAddr);
    (*env)->DeleteLocalRef(env, InetAddressBack);
    if ((*env)->ExceptionCheck(env)) {  // 检查JNI调用是否有引发异常
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);        // 清除引发的异常，在Java层不会打印异常的堆栈信息
        (*env)->ThrowNew(env, (*env)->FindClass(env, "java/lang/Exception"), "JNI抛出的异常！");
    }
    return HostAddress;
}
