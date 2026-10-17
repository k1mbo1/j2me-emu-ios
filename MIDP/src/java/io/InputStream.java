package java.io;

public abstract class InputStream {

    public abstract int read() throws IOException;

    public int read(byte[] b) throws IOException {
        return read(b, 0, b.length);
    }

    public int read(byte[] b, int off, int len) throws IOException {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off + len > b.length) throw new IndexOutOfBoundsException();
        int n = 0;
        while (n < len) {
            int c = read();
            if (c < 0) break;
            b[off + n++] = (byte) c;
        }
        return n == 0 && len > 0 ? -1 : n;
    }

    public long skip(long n) throws IOException {
        long s = 0;
        while (s < n) {
            if (read() < 0) break;
            s++;
        }
        return s;
    }

    public int available() throws IOException { return 0; }

    public void close() throws IOException { }
}
