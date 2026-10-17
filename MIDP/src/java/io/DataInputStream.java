package java.io;

public class DataInputStream extends InputStream implements DataInput {
    protected InputStream in;

    public DataInputStream(InputStream in) { this.in = in; }

    public int read() throws IOException { return in.read(); }
    public int read(byte[] b) throws IOException { return in.read(b); }
    public int read(byte[] b, int off, int len) throws IOException { return in.read(b, off, len); }
    public long skip(long n) throws IOException { return in.skip(n); }
    public int available() throws IOException { return in.available(); }
    public void close() throws IOException { in.close(); }

    public void readFully(byte[] b) throws IOException { readFully(b, 0, b.length); }

    public void readFully(byte[] b, int off, int len) throws IOException {
        int n = 0;
        while (n < len) {
            int c = in.read(b, off + n, len - n);
            if (c < 0) throw new IOException("EOF");
            n += c;
        }
    }

    public int skipBytes(int n) throws IOException {
        int s = 0;
        while (s < n) {
            long r = in.skip(n - s);
            if (r <= 0) break;
            s += (int) r;
        }
        return s;
    }

    public boolean readBoolean() throws IOException { return readByte() != 0; }

    public byte readByte() throws IOException {
        int c = in.read();
        if (c < 0) throw new IOException("EOF");
        return (byte) c;
    }

    public int readUnsignedByte() throws IOException {
        int c = in.read();
        if (c < 0) throw new IOException("EOF");
        return c;
    }

    public short readShort() throws IOException {
        int hi = readUnsignedByte();
        return (short) ((hi << 8) | readUnsignedByte());
    }

    public int readUnsignedShort() throws IOException {
        int hi = readUnsignedByte();
        return (hi << 8) | readUnsignedByte();
    }

    public char readChar() throws IOException {
        return (char) readUnsignedShort();
    }

    public int readInt() throws IOException {
        return (readUnsignedByte() << 24) | (readUnsignedByte() << 16)
                | (readUnsignedByte() << 8) | readUnsignedByte();
    }

    public long readLong() throws IOException {
        long hi = readInt() & 0xFFFFFFFFL;
        long lo = readInt() & 0xFFFFFFFFL;
        return (hi << 32) | lo;
    }

    public float readFloat() throws IOException { return Float.intBitsToFloat(readInt()); }

    public double readDouble() throws IOException { return Double.longBitsToDouble(readLong()); }

    public String readUTF() throws IOException {
        int len = readUnsignedShort();
        byte[] b = new byte[len];
        readFully(b);
        return new String(b);   /* UTF-8 解码（ASCII 与 MUTF-8 一致） */
    }
}
