package cn.cbsd.n2nsupport;

import android.app.ProgressDialog;
import android.content.Intent;
import android.net.VpnService;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import org.greenrobot.eventbus.EventBus;
import org.greenrobot.eventbus.Subscribe;
import org.greenrobot.eventbus.ThreadMode;
import org.json.JSONException;
import org.json.JSONObject;

import cn.cbsd.vpnx.Event.ConnectEvent;
import cn.cbsd.vpnx.Event.DisconnectEvent;
import cn.cbsd.vpnx.Tool.GetMac;
import cn.cbsd.vpnx.Tool.ServerConnectTool;
import cn.cbsd.vpnx.model.VPNStatus;
import cn.cbsd.vpnx.service.VPNXService;

import static cn.cbsd.vpnx.service.VPNXService.VPNX_login;

public class MainActivity extends AppCompatActivity {

    final static String UrlandSuffix = "http://124.172.232.89:8050/daServer/vpn_jkapp?";
    private static final int REQUECT_CODE_VPN = 1;
    Button btn_openVPN;
    Button btn_getVPNStatus;
    VPNStatus.RunningStatus status;
    private ProgressDialog progressDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        EventBus.getDefault().register(this);
        btn_openVPN = (Button) findViewById(R.id.btn_VPN);
        btn_getVPNStatus = (Button) findViewById(R.id.btn_VPNStatus);
        btn_openVPN.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                status = VPNXService.INSTANCE == null ? VPNStatus.RunningStatus.DISCONNECT : VPNXService.INSTANCE.getVPNStatus();
                if (/*VPNXService.INSTANCE != null && */status != VPNStatus.RunningStatus.DISCONNECT && status != VPNStatus.RunningStatus.FAILED) {
                    VPNXService.INSTANCE.stop();
                } else {
                    final JSONObject jsonData = new JSONObject();
                    try {
                        jsonData.put("logintype", "ukey");
                        jsonData.put("id", "yzb_tscdw1");
                        jsonData.put("password", "1234");
                        jsonData.put("project", "yzb");
                        jsonData.put("token", "4BDF80416DCF8E112C2E32F81D5BBB33");
                    } catch (JSONException e) {
                        e.printStackTrace();
                    }
                    progressDialog = new ProgressDialog(MainActivity.this);
                    progressDialog.setCanceledOnTouchOutside(false);
                    progressDialog.getWindow().getAttributes().gravity = Gravity.CENTER;
                    progressDialog.setMessage("数据上传中，请稍候");
                    progressDialog.show();
                    VPNX_login(UrlandSuffix, jsonData.toString(), GetMac.getMacAddress(), new ServerConnectTool.Callback() {
                        @Override
                        public void onResponse(String response) {
                            progressDialog.dismiss();
                            if (response == null) {
                                Toast.makeText(MainActivity.this, "没有数据返回", Toast.LENGTH_SHORT).show();
                            } else if (response.equals("true")) {
                                Intent vpnPrepareIntent = VpnService.prepare(MainActivity.this);
                                if (vpnPrepareIntent != null) {
                                    startActivityForResult(vpnPrepareIntent, REQUECT_CODE_VPN);
                                } else {
                                    onActivityResult(REQUECT_CODE_VPN, RESULT_OK, null);
                                }
                            }else{
                                Toast.makeText(MainActivity.this, response, Toast.LENGTH_SHORT).show();
                            }
                        }
                    });
                }
            }
        });
        btn_getVPNStatus.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                status = VPNXService.INSTANCE == null ? VPNStatus.RunningStatus.DISCONNECT : VPNXService.INSTANCE.getVPNStatus();
                switch (status) {
                    case FAILED:
                        Toast.makeText(MainActivity.this, "FAILED", Toast.LENGTH_LONG).show();
                        break;
                    case CONNECTED:
                        Toast.makeText(MainActivity.this, "CONNECTED", Toast.LENGTH_LONG).show();
                        break;
                    case CONNECTING:
                        Toast.makeText(MainActivity.this, "CONNECTING", Toast.LENGTH_LONG).show();
                        break;
                    case DISCONNECT:
                        Toast.makeText(MainActivity.this, "DISCONNECT", Toast.LENGTH_LONG).show();
                        break;
                    case NODE_DISCONNECT:
                        Toast.makeText(MainActivity.this, "SUPERNODE_DISCONNECT", Toast.LENGTH_LONG).show();
                        break;
                    default:
                        break;
                }
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    Intent intent;

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUECT_CODE_VPN && resultCode == RESULT_OK) {
            intent = new Intent(MainActivity.this, VPNXService.class);
            startService(intent);
        }
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void OnGetStartEvent(ConnectEvent event) {
        Toast.makeText(MainActivity.this, "CONNECTED", Toast.LENGTH_LONG).show();
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void DisconnectEvent(DisconnectEvent event) {
        Toast.makeText(MainActivity.this, "DISCONNECTED", Toast.LENGTH_LONG).show();
    }

    @Subscribe(threadMode = ThreadMode.MAIN)
    public void OnGetErrorEvent(Error event) {
        Toast.makeText(MainActivity.this, "ERROR", Toast.LENGTH_LONG).show();
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();
        EventBus.getDefault().unregister(this);
        if (intent != null) {
            VPNXService.INSTANCE.stop();
            stopService(intent);
        }
    }

}
