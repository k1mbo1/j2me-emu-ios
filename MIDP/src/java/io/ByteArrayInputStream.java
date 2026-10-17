package java.io;

public class ByteArrayInputStream extends InputStream {
    protected byte[] buf;
    protected int pos;
    protected int count;
    protected int mark;

    public ByteArrayInputStream(byte[] b) {
        buf = b; pos = 0; count = b.length; mark = 0;
    }

    public ByteArrayInputStream(byte[] b, int off, int len) {
        buf = b; pos = off; count = off + len; mark = off;
    }

    public int read() {
        return pos < count ? (buf[pos++] & 0xFF) : -1;
    }

    public int read(byte[] b, int off, int len) {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off + len > b.length) throw new IndexOutOfBoundsException();
        if (pos >= count) return -1;
        int n = count - pos;
        if (n > len) n = len;
        System.arraycopy(buf, pos, b, off, n);
        pos += n;
        return n;
    }

    public long skip(long n) {
        long s = count - pos;
        if (s > n) s = n;
        if (s < 0) return 0;
        pos += (int) s;
        return s;
    }

    public int available() { return count - pos; }

    public void reset() { pos = mark; }
}
