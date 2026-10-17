package java.lang;

public final class Double extends Number {
    public static final double POSITIVE_INFINITY = 1.0 / 0.0;
    public static final double NEGATIVE_INFINITY = -1.0 / 0.0;
    public static final double NaN = 0.0 / 0.0;
    public static final double MAX_VALUE = 1.7976931348623157e+308;
    public static final double MIN_VALUE = 5.0e-324;

    private final double value;

    public Double(double v) { value = v; }
    public Double(String s) throws NumberFormatException { value = parseDouble(s); }

    public static double parseDouble(String s) throws NumberFormatException {
        if (s == null) throw new NumberFormatException("null");
        s = s.trim();
        int n = s.length();
        if (n == 0) throw new NumberFormatException("empty");
        if (s.equals("NaN")) return 0.0 / 0.0;
        if (s.equals("Infinity") || s.equals("+Infinity")) return 1.0 / 0.0;
        if (s.equals("-Infinity")) return -1.0 / 0.0;

        int i = 0;
        boolean neg = false;
        if (s.charAt(0) == '+' || s.charAt(0) == '-') {
            neg = s.charAt(0) == '-';
            i = 1;
        }

        long mant = 0;
        int scale = 0;
        int expVal = 0;
        boolean expNeg = false;
        boolean sawDigit = false;
        boolean sawDot = false;
        boolean inExp = false;

        for (; i < n; i++) {
            char c = s.charAt(i);
            if (c >= '0' && c <= '9') {
                sawDigit = true;
                if (inExp) {
                    if (expVal < 100000) expVal = expVal * 10 + (c - '0');
                } else if (mant < 1000000000000000L) {
                    mant = mant * 10 + (c - '0');
                    if (sawDot) scale++;
                } else if (!sawDot) {
                    scale--;
                }
            } else if (c == '.' && !sawDot && !inExp) {
                sawDot = true;
            } else if ((c == 'e' || c == 'E') && sawDigit && !inExp) {
                inExp = true;
                if (i + 1 < n && (s.charAt(i + 1) == '+' || s.charAt(i + 1) == '-')) {
                    expNeg = s.charAt(i + 1) == '-';
                    i++;
                }
            } else {
                throw new NumberFormatException(s);
            }
        }
        if (!sawDigit) throw new NumberFormatException(s);

        double v = mant;
        int e = (expNeg ? -expVal : expVal) - scale;
        if (e != 0) v = v * Math.pow(10.0, e);
        return neg ? -v : v;
    }

    public static boolean isNaN(double v) { return v != v; }
    public static boolean isInfinite(double v) {
        return v == POSITIVE_INFINITY || v == NEGATIVE_INFINITY;
    }

    public static native String toString(double v);
    public static native long doubleToLongBits(double v);
    public static native double longBitsToDouble(long bits);

    public static Double valueOf(double v) { return new Double(v); }
    public static Double valueOf(String s) throws NumberFormatException { return valueOf(parseDouble(s)); }
    public static Double valueOf(float v) { return new Double(v); }

    public boolean isNaN() { return isNaN(value); }
    public boolean isInfinite() { return isInfinite(value); }

    public byte byteValue() { return (byte) value; }
    public short shortValue() { return (short) value; }
    public int intValue() { return (int) value; }
    public long longValue() { return (long) value; }
    public float floatValue() { return (float) value; }
    public double doubleValue() { return value; }

    public String toString() { return toString(value); }
    public boolean equals(Object o) { return (o instanceof Double) && ((Double) o).value == value; }
    public int hashCode() { return (int) value; }
}
