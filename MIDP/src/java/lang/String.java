package java.lang;

/**
 * 骨架 String：字段 value(char[]) 布局被 C 层 vmG.fldStringValue 依赖，勿改名。
 * intern() 为 native。
 */
public final class String {
    char[] value;
    int hash;

    public String() { value = new char[0]; }

    public String(String s) {
        value = s.value;
        hash = s.hash;
    }

    public String(char[] v) {
        value = new char[v.length];
        System.arraycopy(v, 0, value, 0, v.length);
    }

    public String(char[] v, int off, int len) {
        if (off < 0 || len < 0 || off + len > v.length)
            throw new StringIndexOutOfBoundsException();
        value = new char[len];
        System.arraycopy(v, off, value, 0, len);
    }

    public String(byte[] b, String enc) throws java.io.UnsupportedEncodingException {
        if (!"UTF-8".equals(enc) && !"utf-8".equals(enc) && !"UTF8".equals(enc)
                && !"ISO-8859-1".equals(enc))
            throw new java.io.UnsupportedEncodingException(enc);
        value = decodeUtf8(b, 0, b.length);
    }

    public String(byte[] b) {
        value = decodeUtf8(b, 0, b.length);
    }

    public String(byte[] b, int off, int len) {
        value = decodeUtf8(b, off, len);
    }

    /** 指定字符集 + 偏移长度的构造（游戏读文本资源常用；GBK 按 UTF-8 宽松解码） */
    public String(byte[] b, int off, int len, String enc)
            throws java.io.UnsupportedEncodingException {
        if (enc == null) throw new NullPointerException();
        if (!"UTF-8".equals(enc) && !"utf-8".equals(enc) && !"UTF8".equals(enc)
                && !"GBK".equalsIgnoreCase(enc) && !"gb2312".equalsIgnoreCase(enc)
                && !"GB2312".equals(enc) && !"ISO-8859-1".equalsIgnoreCase(enc))
            throw new java.io.UnsupportedEncodingException(enc);
        value = decodeUtf8(b, off, len);
    }

    public String(StringBuffer sb) {
        synchronized (sb) {
            value = new char[sb.length()];
            sb.getChars(0, sb.length(), value, 0);
        }
    }

    private static char[] decodeUtf8(byte[] b, int off, int len) {
        if (off < 0 || len < 0 || off + len > b.length)
            throw new StringIndexOutOfBoundsException();
        char[] out = new char[len];
        int n = 0, i = off, end = off + len;
        while (i < end) {
            int c = b[i] & 0xFF;
            if (c < 0x80) { out[n++] = (char) c; i += 1; }
            else if ((c & 0xE0) == 0xC0 && i + 1 < end) {
                out[n++] = (char) (((c & 0x1F) << 6) | (b[i + 1] & 0x3F)); i += 2;
            } else if ((c & 0xF0) == 0xE0 && i + 2 < end) {
                out[n++] = (char) (((c & 0x0F) << 12) | ((b[i + 1] & 0x3F) << 6) | (b[i + 2] & 0x3F));
                i += 3;
            } else { out[n++] = '\uFFFD'; i += 1; }
        }
        if (n == out.length) return out;
        char[] r = new char[n];
        System.arraycopy(out, 0, r, 0, n);
        return r;
    }

    public int length() { return value.length; }

    public char charAt(int i) {
        if (i < 0 || i >= value.length)
            throw new StringIndexOutOfBoundsException(String.valueOf(i));
        return value[i];
    }

    public void getChars(int srcBegin, int srcEnd, char[] dst, int dstBegin) {
        if (srcBegin < 0 || srcEnd > value.length || srcBegin > srcEnd
                || dstBegin < 0 || dstBegin + (srcEnd - srcBegin) > dst.length)
            throw new StringIndexOutOfBoundsException();
        System.arraycopy(value, srcBegin, dst, dstBegin, srcEnd - srcBegin);
    }

    public char[] toCharArray() {
        char[] r = new char[value.length];
        System.arraycopy(value, 0, r, 0, value.length);
        return r;
    }

    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof String)) return false;
        String s = (String) o;
        if (s.value.length != value.length) return false;
        for (int i = 0; i < value.length; i++)
            if (s.value[i] != value[i]) return false;
        return true;
    }

    public boolean equalsIgnoreCase(String s) {
        if (s == null || s.value.length != value.length) return false;
        for (int i = 0; i < value.length; i++) {
            char a = Character.toUpperCase(value[i]);
            char b = Character.toUpperCase(s.value[i]);
            if (a != b) return false;
        }
        return true;
    }

    public int hashCode() {
        int h = hash;
        if (h == 0 && value.length > 0) {
            for (int i = 0; i < value.length; i++)
                h = 31 * h + value[i];
            hash = h;
        }
        return h;
    }

    public native String intern();

    public String toString() { return this; }

    public int compareTo(String s) {
        int n = Math.min(value.length, s.value.length);
        for (int i = 0; i < n; i++) {
            if (value[i] != s.value[i]) return value[i] - s.value[i];
        }
        return value.length - s.value.length;
    }

    public boolean startsWith(String p) { return startsWith(p, 0); }

    public boolean startsWith(String p, int off) {
        if (off < 0 || off + p.value.length > value.length) return false;
        for (int i = 0; i < p.value.length; i++)
            if (value[off + i] != p.value[i]) return false;
        return true;
    }

    public boolean endsWith(String p) {
        return startsWith(p, value.length - p.value.length);
    }

    public int indexOf(int ch) { return indexOf(ch, 0); }

    public int indexOf(int ch, int from) {
        for (int i = from; i < value.length; i++)
            if (value[i] == (char) ch) return i;
        return -1;
    }

    public int indexOf(String s) { return indexOf(s, 0); }

    public int indexOf(String s, int from) {
        if (from < 0) from = 0;
        outer:
        for (int i = from; i + s.value.length <= value.length; i++) {
            for (int k = 0; k < s.value.length; k++)
                if (value[i + k] != s.value[k]) continue outer;
            return i;
        }
        return -1;
    }

    public int lastIndexOf(int ch) {
        for (int i = value.length - 1; i >= 0; i--)
            if (value[i] == (char) ch) return i;
        return -1;
    }

    public int lastIndexOf(String s) {
        outer:
        for (int i = value.length - s.value.length; i >= 0; i--) {
            for (int k = 0; k < s.value.length; k++)
                if (value[i + k] != s.value[k]) continue outer;
            return i;
        }
        return -1;
    }

    public String substring(int b) {
        return substring(b, value.length);
    }

    public String substring(int b, int e) {
        if (b < 0 || e > value.length || b > e)
            throw new StringIndexOutOfBoundsException();
        char[] r = new char[e - b];
        System.arraycopy(value, b, r, 0, e - b);
        return new String(r);
    }

    public String concat(String s) {
        char[] r = new char[value.length + s.value.length];
        System.arraycopy(value, 0, r, 0, value.length);
        System.arraycopy(s.value, 0, r, value.length, s.value.length);
        return new String(r);
    }

    public String replace(char a, char b) {
        char[] r = new char[value.length];
        for (int i = 0; i < value.length; i++)
            r[i] = value[i] == a ? b : value[i];
        return new String(r);
    }

    public String trim() {
        int b = 0, e = value.length - 1;
        while (b <= e && value[b] <= ' ') b++;
        while (e >= b && value[e] <= ' ') e--;
        return substring(b, e + 1);
    }

    public String toUpperCase() {
        char[] r = new char[value.length];
        for (int i = 0; i < value.length; i++)
            r[i] = Character.toUpperCase(value[i]);
        return new String(r);
    }

    public String toLowerCase() {
        char[] r = new char[value.length];
        for (int i = 0; i < value.length; i++)
            r[i] = Character.toLowerCase(value[i]);
        return new String(r);
    }

    public byte[] getBytes(String enc) throws java.io.UnsupportedEncodingException {
        if (!"UTF-8".equals(enc) && !"utf-8".equals(enc) && !"UTF8".equals(enc)
                && !"ISO-8859-1".equals(enc))
            throw new java.io.UnsupportedEncodingException(enc);
        return encodeUtf8();
    }

    public byte[] getBytes() { return encodeUtf8(); }

    private byte[] encodeUtf8() {
        byte[] out = new byte[value.length * 3];
        int n = 0;
        for (int i = 0; i < value.length; i++) {
            char c = value[i];
            if (c < 0x80) {
                out[n++] = (byte) c;
            } else if (c < 0x800) {
                out[n++] = (byte) (0xC0 | (c >> 6));
                out[n++] = (byte) (0x80 | (c & 0x3F));
            } else {
                out[n++] = (byte) (0xE0 | (c >> 12));
                out[n++] = (byte) (0x80 | ((c >> 6) & 0x3F));
                out[n++] = (byte) (0x80 | (c & 0x3F));
            }
        }
        byte[] r = new byte[n];
        System.arraycopy(out, 0, r, 0, n);
        return r;
    }

    public static String valueOf(Object o) { return o == null ? "null" : o.toString(); }
    public static String valueOf(char[] v) { return new String(v); }
    public static String valueOf(boolean b) { return b ? "true" : "false"; }
    public static String valueOf(char c) {
        char[] a = new char[1]; a[0] = c; return new String(a);
    }
    public static String valueOf(int i) { return Integer.toString(i); }
    public static String valueOf(long l) { return Long.toString(l); }
    public static String valueOf(float f) { return Float.toString(f); }
    public static String valueOf(double d) { return Double.toString(d); }

    public static String copyValueOf(char[] v) { return new String(v); }
}
