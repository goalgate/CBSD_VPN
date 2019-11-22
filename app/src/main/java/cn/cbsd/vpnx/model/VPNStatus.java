package cn.cbsd.vpnx.model;


public class VPNStatus {
    public enum RunningStatus {
        CONNECTING,
        CONNECTED,
        NODE_DISCONNECT,
        DISCONNECT,
        FAILED
    }

    public RunningStatus runningStatus;
}
