package javax.microedition.io;

import java.io.IOException;

public interface HttpConnection extends ContentConnection {

    String HTTP_1_1 = "HTTP/1.1";
    int GET = 1, POST = 2, HEAD = 4;

    String getURL();
    String getProtocol();
    String getHost();
    String getFile();
    String getRef();
    String getQuery();
    int getPort();
    String getRequestMethod();
    void setRequestMethod(String method) throws IOException;
    String getRequestProperty(String key);
    void setRequestProperty(String key, String value) throws IOException;

    int getResponseCode() throws IOException;
    String getResponseMessage() throws IOException;
    long getExpiration() throws IOException;
    long getDate() throws IOException;
    long getLastModified() throws IOException;
    String getHeaderField(String name) throws IOException;
    int getHeaderFieldInt(String name, int def) throws IOException;
    long getHeaderFieldDate(String name, long def) throws IOException;
    String getHeaderField(int n) throws IOException;
    String getHeaderFieldKey(int n) throws IOException;
}
