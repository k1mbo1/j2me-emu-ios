package java.io;

public class DataOutputStream extends OutputStream implements DataOutput {
    protected OutputStream out;
    protected int written;

    public DataOutputStream(OutputStream out) { this.out = out; }

    public void write(int b) throws IOException { out.write(b); written++; }
    public void write(byte[] b) throws IOException { out.write(b); written += b.length; }
    public void write(byte[] b, int off, int len) throws IOException { out.write(b, off, len); written += len; }
    public void flush() throws IOException { out.flush(); }
    public void close() throws IOException { out.close(); }

    public int size() { return written; }

    public void writeBoolean(boolean v) throws IOException { write(v ? 1 : 0); }
    public void writeByte(int v) throws IOException { write(v); }
    public void writeShort(int v) throws IOException { write((v >>> 8) & 0xFF); write(v & 0xFF); }
    public void writeChar(int v) throws IOException { writeShort(v); }
    public void writeInt(int v) throws IOException {
        write((v >>> 24) & 0xFF); write((v >>> 16) & 0xFF);
        write((v >>> 8) & 0xFF); write(v & 0xFF);
    }
    public void writeLong(long v) throws IOException {
        writeInt((int) (v >>> 32));
        writeInt((int) v);
    }
    public void writeFloat(float v) throws IOException { writeInt(Float.floatToIntBits(v)); }
    public void writeDouble(double v) throws IOException { writeLong(Double.doubleToLongBits(v)); }

    public void writeBytes(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) write((byte) s.charAt(i));
    }

    public void writeChars(String s) throws IOException {
        for (int i = 0; i < s.length(); i++) writeShort(s.charAt(i));
    }

    public void writeUTF(String s) throws IOException {
        byte[] b = s.getBytes();   /* UTF-8（ASCII 与 MUTF-8 一致） */
        if (b.length > 65535) throw new IOException("UTF too long");
        writeShort(b.length);
        write(b);
    }
}
