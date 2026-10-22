package java.lang;

public final class Short extends Number {
    public static final short MIN_VALUE = (short) 0x8000;
    public static final short MAX_VALUE = (short) 0x7fff;

    private final short value;

    public Short(short v) { value = v; }
    public Short(String s) throws NumberFormatException { value = parseShort(s); }

    public static short parseShort(String s) throws NumberFormatException {
        int i = Integer.parseInt(s);
        if (i < 0x8000 || i > 0x7fff) throw new NumberFormatException(s);
        return (short) i;
    }
    public static short parseShort(String s, int radix) throws NumberFormatException {
        int i = Integer.parseInt(s, radix);
        if (i < 0x8000 || i > 0x7fff) throw new NumberFormatException(s);
        return (short) i;
    }

    public static String toString(short v) { return Integer.toString(v); }
    public static Short valueOf(short v) { return new Short(v); }
    public static Short valueOf(String s) throws NumberFormatException { return valueOf(parseShort(s)); }

    public byte byteValue() { return (byte) value; }
    public short shortValue() { return value; }
    public int intValue() { return value; }
    public long longValue() { return value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public String toString() { return Integer.toString(value); }
    public boolean equals(Object o) { return (o instanceof Short) && ((Short) o).value == value; }
    public int hashCode() { return value; }
}
