package javax.microedition.io;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;

/**
 * Connector：J2ME 通用连接入口。
 * v1.0.0 支持 http:// https:// resource:（jar 内资源读取）；其他协议抛 ConnectionNotFoundException。
 * HTTP 走 native httpExecute（Bridge 实现：iOS NSURLSession / Windows WinHTTP）。
 */
public class Connector {

    public static final int READ = 1, WRITE = 2, READ_WRITE = 3;

    private Connector() { }

    public static Connection open(String name) throws IOException {
        return open(name, READ_WRITE);
    }

    public static Connection open(String name, int mode) throws IOException {
        return open(name, mode, false);
    }

    public static Connection open(String name, int mode, boolean timeouts)
            throws IOException {
        if (name == null) throw new NullPointerException();
        if (mode != READ && mode != WRITE && mode != READ_WRITE)
            throw new IllegalArgumentException("mode");
        if (name.startsWith("http://") || name.startsWith("https://"))
            return new HttpConnectionImpl(name);
        if (name.startsWith("resource:")) {
            String res = name.substring("resource:".length());
            if (res.startsWith("/")) res = res.substring(1);
            return new ResourceConnection(res);
        }
        if (name.startsWith("socket://") || name.startsWith("ssl://")
                || name.startsWith("datagram://") || name.startsWith("comm:"))
            throw new ConnectionNotFoundException("protocol not supported: " + name);
        throw new ConnectionNotFoundException("unknown protocol: " + name);
    }

    public static InputStream openInputStream(String name) throws IOException {
        return ((InputConnection) open(name, READ)).openInputStream();
    }

    public static DataInputStream openDataInputStream(String name) throws IOException {
        return new DataInputStream(openInputStream(name));
    }

    public static OutputStream openOutputStream(String name) throws IOException {
        return ((OutputConnection) open(name, WRITE)).openOutputStream();
    }

    public static DataOutputStream openDataOutputStream(String name) throws IOException {
        return new DataOutputStream(openOutputStream(name));
    }

    /* ---- resource: 连接（读 jar 内资源） ---- */
    static class ResourceConnection implements InputConnection {
        private final String res;
        private byte[] data;

        ResourceConnection(String res) { this.res = res; }

        private synchronized byte[] load() throws IOException {
            if (data == null) {
                data = javax.microedition.lcdui.Image.bridgeReadResource(res);
                if (data == null) throw new IOException("resource: " + res);
            }
            return data;
        }

        public InputStream openInputStream() throws IOException {
            return new java.io.ByteArrayInputStream(load());
        }

        public DataInputStream openDataInputStream() throws IOException {
            return new DataInputStream(openInputStream());
        }

        public void close() { }
    }

    /* ---- HTTP 实现（同步阻塞，native 执行） ---- */
    static class HttpConnectionImpl implements HttpConnection {
        private final String url;
        private int method = GET;
        private final java.util.Vector keys = new java.util.Vector();
        private final java.util.Vector vals = new java.util.Vector();
        private byte[] postBody;

        /* 响应 */
        private boolean executed;
        private int respCode;
        private String respMsg;
        private byte[] respHeaders;   /* "K\r\nV\r\nK\r\nV..." UTF-8 */
        private byte[] respBody;
        private String type;
        private String encoding;
        private long length = -1;

        HttpConnectionImpl(String url) { this.url = url; }

        public String getURL() { return url; }
        public String getProtocol() { return url.startsWith("https") ? "https" : "http"; }
        public String getHost() {
            int s = url.indexOf("//") + 2;
            int e = url.indexOf('/', s);
            if (e < 0) e = url.length();
            String hp = url.substring(s, e);
            int c = hp.indexOf(':');
            return c < 0 ? hp : hp.substring(0, c);
        }
        public int getPort() {
            int s = url.indexOf("//") + 2;
            int e = url.indexOf('/', s);
            if (e < 0) e = url.length();
            String hp = url.substring(s, e);
            int c = hp.indexOf(':');
            return c < 0 ? (getProtocol().equals("https") ? 443 : 80)
                    : Integer.parseInt(hp.substring(c + 1));
        }
        public String getFile() {
            int s = url.indexOf("//") + 2;
            int e = url.indexOf('/', s);
            if (e < 0) return "";
            String f = url.substring(e);
            int q = f.indexOf('#');
            return q < 0 ? f : f.substring(0, q);
        }
        public String getQuery() {
            String f = getFile();
            int q = f.indexOf('?');
            return q < 0 ? null : f.substring(q + 1);
        }
        public String getRef() {
            int q = url.indexOf('#');
            return q < 0 ? null : url.substring(q + 1);
        }

        public String getRequestMethod() {
            switch (method) {
                case POST: return "POST";
                case HEAD: return "HEAD";
                default: return "GET";
            }
        }

        public void setRequestMethod(String m) throws IOException {
            if ("GET".equals(m)) method = GET;
            else if ("POST".equals(m)) method = POST;
            else if ("HEAD".equals(m)) method = HEAD;
            else throw new IOException("bad method");
        }

        public String getRequestProperty(String key) {
            for (int i = 0; i < keys.size(); i++)
                if (((String) keys.elementAt(i)).equalsIgnoreCase(key))
                    return (String) vals.elementAt(i);
            return null;
        }

        public void setRequestProperty(String key, String value) throws IOException {
            if (key == null) throw new NullPointerException();
            for (int i = 0; i < keys.size(); i++)
                if (((String) keys.elementAt(i)).equalsIgnoreCase(key)) {
                    vals.setElementAt(value, i);
                    return;
                }
            keys.addElement(key);
            vals.addElement(value);
        }

        private synchronized void ensureExecuted() throws IOException {
            if (executed) return;
            executed = true;
            /* 打包 headers: "k\r\nv\r\n..." */
            StringBuffer hb = new StringBuffer();
            for (int i = 0; i < keys.size(); i++)
                hb.append((String) keys.elementAt(i)).append("\r\n")
                  .append((String) vals.elementAt(i)).append("\r\n");
            byte[] result = nativeHttpExecute(url, getRequestMethod(),
                    hb.toString(), postBody);
            if (result == null) throw new IOException("http failed: " + url);
            /* 解包: code(u2) msgLen(u2) msg hdrLen(u4) hdr bodyLen(u4) body */
            int p = 0;
            respCode = ((result[p] & 0xFF) << 8) | (result[p + 1] & 0xFF); p += 2;
            int msgLen = ((result[p] & 0xFF) << 8) | (result[p + 1] & 0xFF); p += 2;
            respMsg = new String(result, p, msgLen); p += msgLen;
            int hdrLen = ((result[p] & 0xFF) << 24) | ((result[p + 1] & 0xFF) << 16)
                    | ((result[p + 2] & 0xFF) << 8) | (result[p + 3] & 0xFF); p += 4;
            respHeaders = new byte[hdrLen];
            System.arraycopy(result, p, respHeaders, 0, hdrLen); p += hdrLen;
            int bodyLen = ((result[p] & 0xFF) << 24) | ((result[p + 1] & 0xFF) << 16)
                    | ((result[p + 2] & 0xFF) << 8) | (result[p + 3] & 0xFF); p += 4;
            respBody = new byte[bodyLen];
            System.arraycopy(result, p, respBody, 0, bodyLen);
            /* 常用头 */
            String ct = getHeaderField("Content-Type");
            if (ct != null) {
                int sc = ct.indexOf(';');
                type = sc < 0 ? ct : ct.substring(0, sc).trim();
                int cs = ct.indexOf("charset=");
                if (cs >= 0) {
                    String enc = ct.substring(cs + 8).trim();
                    int end2 = enc.indexOf(';');
                    encoding = end2 < 0 ? enc : enc.substring(0, end2).trim();
                }
            }
            String cl = getHeaderField("Content-Length");
            if (cl != null) {
                try { length = Long.parseLong(cl); } catch (NumberFormatException e) { }
            }
        }

        public int getResponseCode() throws IOException { ensureExecuted(); return respCode; }
        public String getResponseMessage() throws IOException { ensureExecuted(); return respMsg; }
        public long getExpiration() throws IOException { return 0; }
        public long getDate() throws IOException { return 0; }
        public long getLastModified() throws IOException { return 0; }

        public String getHeaderField(String name) throws IOException {
            ensureExecuted();
            String h = new String(respHeaders);
            int i = 0;
            while (i < h.length()) {
                int e = h.indexOf("\r\n", i);
                if (e < 0) break;
                String line = h.substring(i, e);
                i = e + 2;
                int c = line.indexOf(':');
                if (c > 0 && line.substring(0, c).trim().equalsIgnoreCase(name))
                    return line.substring(c + 1).trim();
            }
            return null;
        }

        public int getHeaderFieldInt(String name, int def) throws IOException {
            String v = getHeaderField(name);
            if (v == null) return def;
            try { return Integer.parseInt(v); } catch (NumberFormatException e) { return def; }
        }

        public long getHeaderFieldDate(String name, long def) throws IOException {
            return def;
        }

        public String getHeaderFieldKey(int n) throws IOException {
            ensureExecuted();
            String h = new String(respHeaders);
            int i = 0, idx = 0;
            while (i < h.length()) {
                int e = h.indexOf("\r\n", i);
                if (e < 0) break;
                if (idx == n) {
                    String line = h.substring(i, e);
                    int c = line.indexOf(':');
                    return c > 0 ? line.substring(0, c).trim() : null;
                }
                i = e + 2;
                idx++;
            }
            return null;
        }

        public String getHeaderField(int n) throws IOException {
            ensureExecuted();
            String h = new String(respHeaders);
            int i = 0, idx = 0;
            while (i < h.length()) {
                int e = h.indexOf("\r\n", i);
                if (e < 0) break;
                if (idx == n) {
                    String line = h.substring(i, e);
                    int c = line.indexOf(':');
                    return c > 0 ? line.substring(c + 1).trim() : line;
                }
                i = e + 2;
                idx++;
            }
            return null;
        }

        public String getType() { try { ensureExecuted(); } catch (IOException e) { } return type; }
        public String getEncoding() { try { ensureExecuted(); } catch (IOException e) { } return encoding; }
        public long getLength() { try { ensureExecuted(); } catch (IOException e) { } return length; }

        public InputStream openInputStream() throws IOException {
            ensureExecuted();
            return new java.io.ByteArrayInputStream(respBody);
        }

        public DataInputStream openDataInputStream() throws IOException {
            return new DataInputStream(openInputStream());
        }

        public OutputStream openOutputStream() throws IOException {
            return new PostStream();
        }

        public DataOutputStream openDataOutputStream() throws IOException {
            return new DataOutputStream(openOutputStream());
        }

        public void close() { }

        class PostStream extends OutputStream {
            java.io.ByteArrayOutputStream buf = new java.io.ByteArrayOutputStream();
            public void write(int b) { buf.write(b); }
            public void write(byte[] b, int off, int len) { buf.write(b, off, len); }
            public void flush() throws IOException {
                postBody = buf.toByteArray();
            }
        }

        static native byte[] nativeHttpExecute(String url, String method,
                                               String headers, byte[] body);
    }
}
