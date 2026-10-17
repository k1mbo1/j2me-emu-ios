package java.lang;

/** 骨架 StringBuilder（javac -source 8 字符串拼接依赖；与 StringBuffer 同构） */
public final class StringBuilder {
    char[] buf;
    int count;

    public StringBuilder() { buf = new char[16]; }
    public StringBuilder(int cap) { buf = new char[cap < 16 ? 16 : cap]; }
    public StringBuilder(String s) { buf = new char[s.length() + 16]; append(s); }

    private void ensure(int extra) {
        int need = count + extra;
        if (need <= buf.length) return;
        int nc = buf.length * 2;
        if (nc < need) nc = need;
        char[] nb = new char[nc];
        System.arraycopy(buf, 0, nb, 0, count);
        buf = nb;
    }

    public int length() { return count; }
    public int capacity() { return buf.length; }

    public StringBuilder append(String s) {
        if (s == null) s = "null";
        int n = s.length();
        ensure(n);
        s.getChars(0, n, buf, count);
        count += n;
        return this;
    }
    public StringBuilder append(Object o) { return append(o == null ? "null" : o.toString()); }
    public StringBuilder append(char[] c) {
        if (c == null) return append("null");
        ensure(c.length);
        System.arraycopy(c, 0, buf, count, c.length);
        count += c.length;
        return this;
    }
    public StringBuilder append(char c) { ensure(1); buf[count++] = c; return this; }
    public StringBuilder append(boolean b) { return append(b ? "true" : "false"); }
    public StringBuilder append(int i) { return append(Integer.toString(i)); }
    public StringBuilder append(long l) { return append(Long.toString(l)); }
    public StringBuilder append(float f) { return append(Float.toString(f)); }
    public StringBuilder append(double d) { return append(Double.toString(d)); }

    public String toString() { return new String(buf, 0, count); }

    public char charAt(int i) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException(String.valueOf(i));
        return buf[i];
    }
    public void getChars(int b, int e, char[] dst, int off) {
        if (b < 0 || e > count || b > e || off < 0 || off + (e - b) > dst.length)
            throw new StringIndexOutOfBoundsException();
        System.arraycopy(buf, b, dst, off, e - b);
    }
    public void setCharAt(int i, char c) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException();
        buf[i] = c;
    }
    public StringBuilder deleteCharAt(int i) {
        if (i < 0 || i >= count) throw new StringIndexOutOfBoundsException();
        System.arraycopy(buf, i + 1, buf, i, count - i - 1);
        count--;
        return this;
    }
    public StringBuilder delete(int b, int e) {
        if (b < 0 || b > count || b > e) throw new StringIndexOutOfBoundsException();
        if (e > count) e = count;
        if (b == e) return this;
        System.arraycopy(buf, e, buf, b, count - e);
        count -= (e - b);
        return this;
    }
    public StringBuilder replace(int b, int e, String s) {
        if (b < 0 || b > count || b > e) throw new StringIndexOutOfBoundsException();
        if (e > count) e = count;
        int n = s.length();
        int growth = n - (e - b);
        if (growth > 0) ensure(growth);
        System.arraycopy(buf, e, buf, b + n, count - e);
        s.getChars(0, n, buf, b);
        count += growth;
        return this;
    }
    public StringBuilder insert(int i, String s) {
        if (i < 0 || i > count) throw new StringIndexOutOfBoundsException();
        if (s == null) s = "null";
        int n = s.length();
        ensure(n);
        System.arraycopy(buf, i, buf, i + n, count - i);
        s.getChars(0, n, buf, i);
        count += n;
        return this;
    }
    public StringBuilder insert(int i, char c) {
        if (i < 0 || i > count) throw new StringIndexOutOfBoundsException();
        ensure(1);
        System.arraycopy(buf, i, buf, i + 1, count - i);
        buf[i] = c;
        count++;
        return this;
    }
    public StringBuilder insert(int i, int v) { return insert(i, Integer.toString(v)); }
    public StringBuilder insert(int i, long v) { return insert(i, Long.toString(v)); }
    public StringBuilder insert(int i, boolean v) { return insert(i, v ? "true" : "false"); }
    public StringBuilder insert(int i, float v) { return insert(i, Float.toString(v)); }
    public StringBuilder insert(int i, double v) { return insert(i, Double.toString(v)); }
    public StringBuilder insert(int i, Object o) { return insert(i, o == null ? "null" : o.toString()); }
    public StringBuilder reverse() {
        for (int a = 0, b = count - 1; a < b; a++, b--) {
            char t = buf[a]; buf[a] = buf[b]; buf[b] = t;
        }
        return this;
    }
    public void setLength(int n) {
        if (n < 0) throw new StringIndexOutOfBoundsException();
        if (n > count) ensure(n - count);
        for (int i = count; i < n; i++) buf[i] = '\0';
        count = n;
    }
}
