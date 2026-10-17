package java.lang;

/**
 * 骨架 Class：字段 name（点号形式）被 C 层 vmGetClassObj 依赖，勿改名。
 * 数组类 name 保持描述符形式（如 [Ljava.lang.String;，与 J2SE 行为一致）。
 */
public final class Class {
    String name;

    private Class() { }

    public static native Class forName(String name) throws ClassNotFoundException;

    public native String getName();

    public String toString() { return "class " + name; }
}
