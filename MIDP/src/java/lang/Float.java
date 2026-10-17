package java.lang;

public final class Float extends Number {
    public static final float POSITIVE_INFINITY = 1.0f / 0.0f;
    public static final float NEGATIVE_INFINITY = -1.0f / 0.0f;
    public static final float NaN = 0.0f / 0.0f;
    public static final float MAX_VALUE = 3.4028235e+38f;
    public static final float MIN_VALUE = 1.4e-45f;

    private final float value;

    public Float(float v) { value = v; }
    public Float(double v) { value = (float) v; }
    public Float(String s) throws NumberFormatException { value = parseFloat(s); }

    public static float parseFloat(String s) throws NumberFormatException {
        return (float) Double.parseDouble(s);
    }

    public static boolean isNaN(float v) { return v != v; }
    public static boolean isInfinite(float v) {
        return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY;
    }

    public static native String toString(float v);
    public static native int floatToIntBits(float v);
    public static native float intBitsToFloat(int bits);

    public static Float valueOf(float v) { return new Float(v); }
    public static Float valueOf(String s) throws NumberFormatException { return valueOf(parseFloat(s)); }
    public static Float valueOf(double v) { return new Float((float) v); }

    public boolean isNaN() { return isNaN(value); }
    public boolean isInfinite() { return isInfinite(value); }

    public byte byteValue() { return (byte) value; }
    public short shortValue() { return (short) value; }
    public int intValue() { return (int) value; }
    public long longValue() { return (long) value; }
    public float floatValue() { return value; }
    public double doubleValue() { return value; }

    public String toString() { return toString(value); }
    public boolean equals(Object o) { return (o instanceof Float) && ((Float) o).value == value; }
    public int hashCode() { return (int) value; }
}
