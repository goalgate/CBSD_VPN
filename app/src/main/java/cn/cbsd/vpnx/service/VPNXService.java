package cn.cbsd.vpnx.service;

import android.content.Intent;
import android.net.VpnService;
import org.greenrobot.eventbus.EventBus;
import cn.cbsd.vpnx.Event.DisconnectEvent;
import cn.cbsd.vpnx.Event.ErrorEvent;
import cn.cbsd.vpnx.Event.ConnectEvent;
import cn.cbsd.vpnx.Event.FailedEvent;
import cn.cbsd.vpnx.Tool.ServerConnectTool;
import cn.cbsd.vpnx.model.VPNStatus;




public class VPNXService extends VpnService {

    static {
        System.loadLibrary("cbsd_log");
        System.loadLibrary("vpnHelper");
        System.loadLibrary("vpnSupport_v3");
        System.loadLibrary("vpnSupport_v2");
        System.loadLibrary("vpnSupport_v1");
        System.loadLibrary("vpn_v3");
        System.loadLibrary("vpn_v2");
        System.loadLibrary("vpn_v1");
        System.loadLibrary("cbsd_vpnx");
    }

    public static VPNXService INSTANCE;

    @Override
    public void onCreate() {
        super.onCreate();
        INSTANCE = this;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {

        if (intent == null) {
            EventBus.getDefault().post(new ErrorEvent());
            return super.onStartCommand(intent, flags, startId);
        }
        try {
            if (!vpnOpen()) {
                EventBus.getDefault().post(new ErrorEvent());
            }
        } catch (Exception e) {
            EventBus.getDefault().post(new ErrorEvent());
        }

        return super.onStartCommand(intent, flags, startId);
    }

    public void stop() {
        vpnClose();
        EventBus.getDefault().post(new DisconnectEvent());
    }

    @Override
    public void onRevoke() {
        super.onRevoke();
        stop();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stop();
    }


    public native boolean vpnOpen();

    public native void vpnClose();

    public static native VPNStatus.RunningStatus getVPNStatus();

    public native static void VPNX_login(String server ,String jsonData,String Mac, ServerConnectTool.Callback callback);

    public native static void getResult();

    //调用 vpnOpen 后由JNI回调的方法，用于自动返回开启的状态，方法名不可变动，方法内容可以修改
    public void reportVPNStatus(VPNStatus status) {
        switch (status.runningStatus) {
            case CONNECTING:
            case CONNECTED:
                EventBus.getDefault().post(new ConnectEvent());
                break;
            case NODE_DISCONNECT:
//                EventBus.getDefault().post(new DisconnectEvent);
                break;
            case DISCONNECT:
                EventBus.getDefault().post(new DisconnectEvent());
                break;
            case FAILED:
                EventBus.getDefault().post(new FailedEvent());
                break;
            default:
                break;
        }
    }

}
