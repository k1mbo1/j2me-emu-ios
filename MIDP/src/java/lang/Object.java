package java.lang;

/** 骨架根类：hashCode/getClass 由 C 层 native 提供 */
public class Object {
    public String toString() {
        return getClass().getName() + "@" + Integer.toHexString(hashCode());
    }
    public native int hashCode();
    public native final Class getClass();
    public boolean equals(Object o) { return this == o; }
    protected Object clone() throws CloneNotSupportedException {
        throw new CloneNotSupportedException();
    }
    protected void finalize() {}
}
