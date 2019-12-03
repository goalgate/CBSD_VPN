package cn.cbsd.vpnx.Tool;

import android.os.Handler;

import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.UUID;

import cn.cbsd.vpnx.service.VPNXService;


public class ServerConnectTool {

    private String UrlandSuffix;

    private String jsonData;

    private String response;

    private static int TIME_OUT = 20 * 1000;   //超时时间
    private static String CHARSET = "utf-8";
    private BufferedReader in = null;
    private static String BOUNDARY = UUID.randomUUID().toString();  //边界标识   随机生成
    private static String CONTENT_TYPE = "multipart/form-data";
    Handler handler = new Handler();

    public void post() {
        new Thread() {
            @Override
            public void run() {
                response = sendPost();
                handler.post(new Runnable() {
                    @Override
                    public void run() {
                        VPNXService.getResult();
                    }
                });
            }
        }.start();
    }

    private String sendPost() {
        String result = null;
        try {
            URL url = new URL(UrlandSuffix + jsonData);
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setReadTimeout(TIME_OUT);
            conn.setConnectTimeout(TIME_OUT);
            conn.setDoInput(true);  //允许输入流
            conn.setDoOutput(true); //允许输出流
            conn.setUseCaches(false);  //不允许使用缓存
            conn.setRequestMethod("POST");  //请求方式
            conn.setRequestProperty("Charset", CHARSET);  //设置编码
            conn.setRequestProperty("connection", "keep-alive");
            conn.setRequestProperty("Content-Type", CONTENT_TYPE + ";boundary=" + BOUNDARY);
            if (200 == conn.getResponseCode()) {
                in = new BufferedReader(
                        new InputStreamReader(conn.getInputStream()));
                String line;
                while ((line = in.readLine()) != null) {
                    result = line;
                }
            } else {
                result = null;
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
        return result;
    }

    public interface Callback {

        void onResponse(String response);

        void onResponse(JSONObject jsonObject);

    }


}
