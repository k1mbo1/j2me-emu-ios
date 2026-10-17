public class TestMain {
    public static void main(String[] args) {
        System.out.println("== start ==");

        // 1. 字符串拼接链（StringBuilder + ldc + intern）
        String a = "Hello";
        String s = a + " J2ME " + 42 + " " + true + " " + 3.5;
        System.out.println(s);

        // 2. Integer 全家
        int n = Integer.parseInt("123");
        System.out.println(n + 100);
        System.out.println(Integer.toString(-456));
        System.out.println(Integer.toHexString(255));
        System.out.println(Integer.valueOf("789").intValue() + 1);

        // 3. Math native + 纯 Java
        System.out.println(Math.sqrt(2.0));
        System.out.println(Math.max(3, 7));

        // 4. StringBuilder
        StringBuilder sb = new StringBuilder();
        sb.append("abc").append(1).append('-').append(2L).append(false);
        System.out.println(sb.toString());
        System.out.println(sb.reverse().toString());

        // 5. Vector
        java.util.Vector v = new java.util.Vector();
        v.addElement("x");
        v.addElement("y");
        v.addElement("z");
        System.out.println(v.size() + ":" + v.elementAt(1) + ":" + v.contains("z"));
        v.removeElement("y");
        System.out.println(v.size());

        // 6. Hashtable
        java.util.Hashtable h = new java.util.Hashtable();
        h.put("k", "value");
        System.out.println((String) h.get("k") + " " + h.size());

        // 7. 异常（try/catch + NumberFormatException）
        try {
            int x = Integer.parseInt("zz");
            System.out.println("should not reach " + x);
        } catch (NumberFormatException e) {
            System.out.println("caught NFE: " + e.getMessage());
        }

        // 8. 强转 + instanceof + Class.forName
        Object o = "mixedCase";
        if (o instanceof String) {
            System.out.println(((String) o).toUpperCase());
        }
        try {
            Class c = Class.forName("java.lang.String");
            System.out.println(c.getName());
        } catch (ClassNotFoundException e) {
            System.out.println("CNFE!");
        }

        // 9. long/double 运算
        long big = 9000000000L;
        System.out.println(big * 2 + 1);
        double d = 1.5;
        System.out.println(d * 2.0 + 0.25);

        // 10. 数组
        int[] arr = new int[5];
        int sum = 0;
        for (int i = 0; i < arr.length; i++) { arr[i] = i * i; sum += arr[i]; }
        System.out.println(sum);
        String[] sa = {"aa", "bb"};
        System.out.println(sa[1].length());

        // 11. Random（种子固定，结果应与标准 Java 一致）
        java.util.Random r = new java.util.Random(42);
        System.out.println(r.nextInt(100) + r.nextInt());

        // 12. StringBuffer + Thread
        StringBuffer sbuf = new StringBuffer("head");
        sbuf.append('-').append("less");
        System.out.println(sbuf.toString());
        System.out.println(Thread.currentThread().getName());

        System.out.println("== done ==");
    }
}
