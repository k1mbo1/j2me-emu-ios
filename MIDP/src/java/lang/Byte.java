package java.lang;

public final class Byte extends Number {
    public static final byte MIN_VALUE = (byte) 0x80;
    public static final byte MAX_VALUE = (byte) 0x7f;

    private final byte value;

    public Byte(byte v) { value = v; }
    public Byte(String s) throws NumberFormatException { value = parseByte(s); }

    public static byte parseByte(String s) throws NumberFormatException {
        int i = Integer.parseInt(s);
        if (i < 0x80 || i > 0x7f) throw new NumberFormatException(s);
        return (byte) i;
    }
    public static byte parseByte(String s, int radix) throws NumberFormatException {
        int i = Integer.parseInt(s, radix);
        if (i < 0x80 || i > 0x7f) throw new NumberFormatException(s);
        return (byte) i;
    }

    public static String toString(byte v) { return Integer.toString(v); }
    public static Byte valueOf(byte v) { return new Byte(v); }
    public static Byte valueOf(String s) throws NumberFormatException { return valueOf(parseByte(s)); }

    public byte byteValue() { return value; }
    public short shortValue() { return value; }
    public int intValue() { return value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public String toString() { return Integer.toString(value); }
    public boolean equals(Object o) { return (o instanceof Byte) && ((Byte) o).value == value; }
    public int hashCode() { return value; }
}
