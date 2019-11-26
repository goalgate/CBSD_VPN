package cn.cbsd.vpnx.Tool;
import android.util.Log;

import java.security.SecureRandom;

import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.DESKeySpec;

public class VPNLoginKey {

	private static String ENCRYPT_KEY = null;

	private final static String DES = "DES";

	/**
	 * 加密
	 *
	 * @param src
	 * 数据源
	 * @param key
	 * 密钥，长度必须是8的倍数
	 * @return 返回加密后的数据
	 * @throws Exception
	 */
	public  byte[] encrypt(byte[] src, byte[] key) throws Exception {
		// DES算法要求有一个可信任的随机数源
		SecureRandom sr = new SecureRandom();
		// 从原始密匙数据创建DESKeySpec对象
		DESKeySpec dks = new DESKeySpec(key);
		// 创建一个密匙工厂，然后用它把DESKeySpec转换成
		// 一个SecretKey对象
		SecretKeyFactory keyFactory = SecretKeyFactory.getInstance(DES);
		SecretKey securekey = keyFactory.generateSecret(dks);
		// Cipher对象实际完成加密操作
		Cipher cipher = Cipher.getInstance(DES);
		// 用密匙初始化Cipher对象
		cipher.init(Cipher.ENCRYPT_MODE, securekey, sr);
		// 现在，获取数据并加密
		// 正式执行加密操作
		return cipher.doFinal(src);
	}

	public  String encryptStr(String src){
		String key="zbvpn.login";
		String ss="";
		try
		{
			byte[] by=encrypt(src.getBytes(),key.getBytes());
			ss=parseByte2HexStr(by);
		}catch(Exception ex)
		{
			ss="";
		}
		return ss;
	}

	/**
	 * 解密
	 *
	 * @param src
	 * 数据源
	 * @param key
	 * 密钥，长度必须是8的倍数
	 * @return 返回解密后的原始数据
	 * @throws Exception
	 */


	public  byte[] decrypt(byte[] src, byte[] key) {
		// DES算法要求有一个可信任的随机数源
		try
		{
			SecureRandom sr = new SecureRandom();
			// 从原始密匙数据创建一个DESKeySpec对象
			DESKeySpec dks = new DESKeySpec(key);
			// 创建一个密匙工厂，然后用它把DESKeySpec对象转换成
			// 一个SecretKey对象
			SecretKeyFactory keyFactory = SecretKeyFactory.getInstance(DES);
			SecretKey securekey = keyFactory.generateSecret(dks);
			// Cipher对象实际完成解密操作
			Cipher cipher = Cipher.getInstance(DES);
			// 用密匙初始化Cipher对象
			cipher.init(Cipher.DECRYPT_MODE, securekey, sr);
			// 现在，获取数据并解密
			// 正式执行解密操作
			return cipher.doFinal(src);
		}catch(Exception ex)
		{
			return null;
		}
	}


	public  String decryptStr(String src){
		String key="zbvpn.login";
		String ss="";
		try
		{
			byte[] bs=parseHexStr2Byte(src);
			byte[] by=decrypt(bs,key.getBytes());
			ss=new String(by);
		}catch(Exception ex)
		{
			ss="";
		}
		return ss;
	}


	/**将二进制转换成16进制
	 * @param buf
	 * @return
	 */
	public  String parseByte2HexStr(byte buf[]) {
		StringBuffer sb = new StringBuffer();
		for (int i = 0; i < buf.length; i++) {
			String hex = Integer.toHexString(buf[i] & 0xFF);
			if (hex.length() == 1) {
				hex = '0' + hex;
			}
			sb.append(hex.toUpperCase());
		}
		return sb.toString();
	}

	/**将16进制转换为二进制
	 * @param hexStr
	 * @return
	 */
	public  byte[] parseHexStr2Byte(String hexStr) {
		if (hexStr.length() < 1)
			return null;
		byte[] result = new byte[hexStr.length()/2];
		for (int i = 0;i< hexStr.length()/2; i++) {
			int high = Integer.parseInt(hexStr.substring(i*2, i*2+1), 16);
			int low = Integer.parseInt(hexStr.substring(i*2+1, i*2+2), 16);
			result[i] = (byte) (high * 16 + low);
		}
		return result;
	}

	/**
	 * 密码解密
	 *
	 * @param data
	 * @return
	 * @throws Exception
	 */
	public   String decrypt(String data) {
		try {
			if (data == null || data.equals(""))
				return data;
			if (ENCRYPT_KEY == null)
				ENCRYPT_KEY ="&&&";
			return new String(decrypt(hex2byte(data.getBytes()), ENCRYPT_KEY.getBytes()));
		} catch (Exception e) {
		}
		return null;
	}

	/**
	 * 密码加密
	 *
	 * @param password
	 * @return
	 * @throws Exception
	 */
	public  String encrypt(String password) {
		try {
			String strc = password;
			strc = new String(strc.getBytes("GB2312"));
			if (ENCRYPT_KEY == null)
				ENCRYPT_KEY ="&&";
			return byte2hex(encrypt(password.getBytes(), ENCRYPT_KEY.getBytes()));
		} catch (Exception e) {
		}
		return null;
	}

	public  String byte2hex(byte[] b) {
		String hs = "";
		String stmp = "";
		for (int n = 0; n < b.length; n++) {
			stmp = (java.lang.Integer.toHexString(b[n] & 0XFF));
			if (stmp.length() == 1)
				hs = hs + "0" + stmp;
			else
				hs = hs + stmp;
		}
		return hs.toUpperCase();
	}

	public  byte[] hex2byte(byte[] b) {
		if ((b.length % 2) != 0)
			throw new IllegalArgumentException("长度不是偶数");
		byte[] b2 = new byte[b.length / 2];
		for (int n = 0; n < b.length; n += 2) {
			String item = new String(b, n, 2);
			b2[n / 2] = (byte) Integer.parseInt(item, 16);
		}
		return b2;
	}


}






