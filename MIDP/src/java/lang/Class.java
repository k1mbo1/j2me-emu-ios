package java.lang;

import java.io.ByteArrayInputStream;
import java.io.InputStream;

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

    /* 游戏加载资源（图片/音频）的标准入口。
     * "/a/b.wav" 绝对路径；"b.wav" 按本类包路径解析。找不到返回 null。 */
    public InputStream getResourceAsStream(String resName) {
        if (resName == null) return null;
        String res = resName;
        if (resName.startsWith("/")) {
            res = resName.substring(1);
        } else {
            int dot = name.lastIndexOf('.');
            if (dot >= 0) {
                String pkg = name.substring(0, dot + 1);   /* "com.foo." */
                StringBuffer sb = new StringBuffer();
                for (int i = 0; i < pkg.length(); i++) {
                    char c = pkg.charAt(i);
                    sb.append(c == '.' ? '/' : c);
                }
                res = sb.append(resName).toString();
            }
        }
        byte[] b = nativeGetResource(res);
        return b == null ? null : new ByteArrayInputStream(b);
    }

    static native byte[] nativeGetResource(String res);
}
