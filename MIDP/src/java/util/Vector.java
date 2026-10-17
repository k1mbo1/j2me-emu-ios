package java.util;

/** 骨架 Vector（CLDC 同款，无锁简化） */
public class Vector {
    protected Object[] elementData;
    protected int elementCount;
    protected int capacityIncrement;

    public Vector() { this(10); }
    public Vector(int initCap) { elementData = new Object[initCap < 0 ? 10 : initCap]; }
    public Vector(int initCap, int incr) {
        elementData = new Object[initCap < 0 ? 10 : initCap];
        capacityIncrement = incr;
    }

    private void ensureCap(int min) {
        if (min <= elementData.length) return;
        int nc = (capacityIncrement > 0)
                ? elementData.length + capacityIncrement
                : elementData.length * 2;
        if (nc < min) nc = min;
        Object[] nd = new Object[nc];
        System.arraycopy(elementData, 0, nd, 0, elementCount);
        elementData = nd;
    }

    public int size() { return elementCount; }
    public boolean isEmpty() { return elementCount == 0; }

    public void addElement(Object o) {
        ensureCap(elementCount + 1);
        elementData[elementCount++] = o;
    }

    public void setElementAt(Object o, int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(i);
        elementData[i] = o;
    }

    public void insertElementAt(Object o, int i) {
        if (i < 0 || i > elementCount) throw new ArrayIndexOutOfBoundsException(i);
        ensureCap(elementCount + 1);
        System.arraycopy(elementData, i, elementData, i + 1, elementCount - i);
        elementData[i] = o;
        elementCount++;
    }

    public boolean removeElement(Object o) {
        int i = indexOf(o);
        if (i >= 0) { removeElementAt(i); return true; }
        return false;
    }

    public void removeElementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(i);
        int moved = elementCount - i - 1;
        if (moved > 0) System.arraycopy(elementData, i + 1, elementData, i, moved);
        elementData[--elementCount] = null;
    }

    public void removeAllElements() {
        for (int i = 0; i < elementCount; i++) elementData[i] = null;
        elementCount = 0;
    }

    public boolean contains(Object o) { return indexOf(o) >= 0; }

    public int indexOf(Object o) { return indexOf(o, 0); }

    public int indexOf(Object o, int start) {
        for (int i = start; i < elementCount; i++) {
            if (o == null ? elementData[i] == null : o.equals(elementData[i])) return i;
        }
        return -1;
    }

    public int lastIndexOf(Object o) {
        for (int i = elementCount - 1; i >= 0; i--) {
            if (o == null ? elementData[i] == null : o.equals(elementData[i])) return i;
        }
        return -1;
    }

    public Object elementAt(int i) {
        if (i < 0 || i >= elementCount) throw new ArrayIndexOutOfBoundsException(i);
        return elementData[i];
    }

    public Object firstElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[0];
    }

    public Object lastElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[elementCount - 1];
    }

    public void copyInto(Object[] a) {
        System.arraycopy(elementData, 0, a, 0, elementCount);
    }

    public void trimToSize() {
        if (elementCount < elementData.length) {
            Object[] nd = new Object[elementCount];
            System.arraycopy(elementData, 0, nd, 0, elementCount);
            elementData = nd;
        }
    }

    public int capacity() { return elementData.length; }

    public void ensureCapacity(int min) { ensureCap(min); }

    public Enumeration elements() {
        Object[] snap = new Object[elementCount];
        System.arraycopy(elementData, 0, snap, 0, elementCount);
        return new VectorEnumerator(snap, elementCount);
    }

    public String toString() {
        StringBuffer sb = new StringBuffer();
        sb.append('[');
        for (int i = 0; i < elementCount; i++) {
            if (i > 0) sb.append(", ");
            sb.append(elementData[i] == null ? "null" : elementData[i].toString());
        }
        sb.append(']');
        return sb.toString();
    }
}
