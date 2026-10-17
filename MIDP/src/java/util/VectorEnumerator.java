package java.util;

/** Vector 专用枚举器（快照视图） */
class VectorEnumerator implements Enumeration {
    private Object[] data;
    private int count;
    private int i = 0;

    VectorEnumerator(Object[] d, int n) {
        data = d;
        count = n;
    }

    public boolean hasMoreElements() { return i < count; }

    public Object nextElement() {
        if (i >= count) throw new NoSuchElementException();
        return data[i++];
    }
}
