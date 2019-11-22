package cn.cbsd.vpnx.Tool;

import java.net.InetAddress;


//JNI内部调用,不能删
public class NetAddressTool {

    public static int getIpAddrPrefixLength(String netmask) {
        try {
            byte[] byteAddr = InetAddress.getByName(netmask).getAddress();
            int prefixLength = 0;
            for (int i = 0; i < byteAddr.length; i++) {
                for (int j = 0; j < 8; j++) {
                    if ((byteAddr[i] << j & 0xFF) != 0) {
                        prefixLength++;
                    } else {
                        return prefixLength;
                    }
                }
            }
            return prefixLength;
        } catch (Exception e) {
            return -1;
        }
    }

    public static String getRoute(String ipAddr, int prefixLength) {
        byte[] arr = {(byte) 0x00, (byte) 0x80, (byte) 0xC0, (byte) 0xE0, (byte) 0xF0, (byte) 0xF8, (byte) 0xFC, (byte) 0xFE, (byte) 0xFF};

        if (prefixLength > 32 || prefixLength < 0) {
            return "";
        }
        try {
            byte[] byteAddr = InetAddress.getByName(ipAddr).getAddress();
            int idx = 0;
            while (prefixLength >= 8) {
                idx++;
                prefixLength -= 8;
            }
            if (idx < byteAddr.length) {
                byteAddr[idx++] &= arr[prefixLength];
            }
            for (; idx < byteAddr.length; idx++) {
                byteAddr[idx] = (byte) 0x00;
            }
            return InetAddress.getByAddress(byteAddr).getHostAddress();
        } catch (Exception e) {
            return "";
        }
    }
}
