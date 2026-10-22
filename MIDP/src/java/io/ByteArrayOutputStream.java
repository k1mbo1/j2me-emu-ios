package java.io;

public class ByteArrayOutputStream extends OutputStream {
    protected byte[] buf;
    protected int count;

    public ByteArrayOutputStream() { this(32); }

    public ByteArrayOutputStream(int size) {
        buf = new byte[size > 0 ? size : 32];
    }

    private void ensure(int n) {
        if (count + n > buf.length) {
            int cap = buf.length * 2;
            while (cap < count + n) cap *= 2;
            byte[] nb = new byte[cap];
            System.arraycopy(buf, 0, nb, 0, count);
            buf = nb;
        }
    }

    public void write(int b) {
        ensure(1);
        buf[count++] = (byte) b;
    }

    public void write(byte[] b, int off, int len) {
        if (b == null) throw new NullPointerException();
        if (off < 0 || len < 0 || off + len > b.length) throw new IndexOutOfBoundsException();
        ensure(len);
        System.arraycopy(b, off, buf, count, len);
        count += len;
    }

    public void reset() { count = 0; }

    public int size() { return count; }

    public byte[] toByteArray() {
        byte[] r = new byte[count];
        System.arraycopy(buf, 0, r, 0, count);
        return r;
    }

    public String toString() { return new String(buf, 0, count); }
}
