package java.io;

/* EOFException：数据流提前结束（DataInputStream 常用） */
public class EOFException extends IOException {
    public EOFException() { super(); }
    public EOFException(String s) { super(s); }
}
