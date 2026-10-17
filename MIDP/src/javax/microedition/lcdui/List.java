package javax.microedition.lcdui;

public class List extends Screen implements Choice {

    private final String[] strings = new String[256];
    private final Image[] images = new Image[256];
    private final boolean[] selected = new boolean[256];
    private int n;
    private int type;
    private int cursor;
    private Command selectCommand;
    private Command implicitCmd;

    public List(String title, int listType) {
        super(title);
        if (listType != IMPLICIT && listType != EXCLUSIVE && listType != MULTIPLE && listType != POPUP)
            throw new IllegalArgumentException();
        this.type = listType;
    }

    public List(String title, int listType, String[] stringElements, Image[] imageElements) {
        this(title, listType);
        for (int i = 0; i < stringElements.length; i++)
            append(stringElements[i], imageElements == null ? null : imageElements[i]);
    }

    public int append(String stringPart, Image imagePart) {
        if (n >= strings.length) throw new IllegalStateException();
        strings[n] = stringPart;
        images[n] = imagePart;
        selected[n] = false;
        n++;
        if (type == IMPLICIT || type == EXCLUSIVE) selected[n - 1] = (n == 1);
        repaintScreen();
        return n - 1;
    }

    public void insert(int elementNum, String stringPart, Image imagePart) {
        if (elementNum < 0 || elementNum > n) throw new IndexOutOfBoundsException();
        int move = n - elementNum;
        if (move > 0) {
            System.arraycopy(strings, elementNum, strings, elementNum + 1, move);
            System.arraycopy(images, elementNum, images, elementNum + 1, move);
            System.arraycopy(selected, elementNum, selected, elementNum + 1, move);
        }
        strings[elementNum] = stringPart;
        images[elementNum] = imagePart;
        selected[elementNum] = false;
        n++;
        repaintScreen();
    }

    public void delete(int elementNum) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        int move = n - elementNum - 1;
        if (move > 0) {
            System.arraycopy(strings, elementNum + 1, strings, elementNum, move);
            System.arraycopy(images, elementNum + 1, images, elementNum, move);
            System.arraycopy(selected, elementNum + 1, selected, elementNum, move);
        }
        n--;
        if (cursor >= n) cursor = n - 1;
        if (cursor < 0) cursor = 0;
        repaintScreen();
    }

    public void deleteAll() {
        n = 0;
        cursor = 0;
        repaintScreen();
    }

    public void set(int elementNum, String stringPart, Image imagePart) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        strings[elementNum] = stringPart;
        images[elementNum] = imagePart;
        repaintScreen();
    }

    public boolean isSelected(int elementNum) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        return selected[elementNum];
    }

    public int getSelectedIndex() {
        if (type == MULTIPLE) return -1;
        for (int i = 0; i < n; i++) if (selected[i]) return i;
        return -1;
    }

    public void setSelectedIndex(int elementNum, boolean sel) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        if (type == MULTIPLE) {
            selected[elementNum] = sel;
        } else if (sel) {
            for (int i = 0; i < n; i++) selected[i] = false;
            selected[elementNum] = true;
        } else if (type == EXCLUSIVE || type == IMPLICIT) {
            /* 单选模式不允许取消唯一选择 */
        }
        repaintScreen();
    }

    public int getSelectedFlags(boolean[] ret) {
        if (ret == null || ret.length < n) throw new IllegalArgumentException();
        int c = 0;
        for (int i = 0; i < n; i++) { ret[i] = selected[i]; if (selected[i]) c++; }
        return c;
    }

    public void setSelectedFlags(boolean[] arr) {
        if (arr == null || arr.length < n) throw new IllegalArgumentException();
        if (type == MULTIPLE) {
            for (int i = 0; i < n; i++) selected[i] = arr[i];
        } else {
            for (int i = 0; i < n; i++)
                if (arr[i]) { setSelectedIndex(i, true); return; }
        }
        repaintScreen();
    }

    public Font getFont(int elementNum) { return Font.getDefaultFont(); }
    public void setFont(int elementNum, Font font) { }
    public int size() { return n; }
    public String getString(int elementNum) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        return strings[elementNum];
    }
    public Image getImage(int elementNum) {
        if (elementNum < 0 || elementNum >= n) throw new IndexOutOfBoundsException();
        return images[elementNum];
    }

    public int fitWidth(int elementNum, Font font) { return 0; }
    public int getFitPolicy() { return 0; }
    public void setFitPolicy(int p) { }
    public void setSelectCommand(Command command) { selectCommand = command; }

    /* ---- 渲染/导航 ---- */
    int paintContent(Graphics g, int x, int y, int w, int h) {
        Font font = Font.getDefaultFont();
        int lineH = font.getHeight() + 6;
        for (int i = 0; i < n; i++) {
            int iy = y + i * lineH;
            /* 高亮选中行 */
            if (i == cursor) {
                g.setColor(0x3050A0);
                g.fillRect(x + 2, iy + 1, w - 6, lineH - 2);
                g.setColor(0xFFFFFF);
            } else {
                g.setColor(0);
            }
            /* 选择符号 */
            int tx = x + 8;
            if (type == MULTIPLE) {
                g.drawRect(x + 6, iy + lineH / 2 - 5, 10, 10);
                if (selected[i]) g.fillRect(x + 8, iy + lineH / 2 - 3, 7, 7);
                tx = x + 24;
            } else if (type == EXCLUSIVE) {
                g.drawArc(x + 6, iy + lineH / 2 - 5, 10, 10, 0, 360);
                if (selected[i]) g.fillArc(x + 8, iy + lineH / 2 - 3, 7, 7, 0, 360);
                tx = x + 24;
            }
            if (strings[i] != null)
                g.drawString(strings[i], tx, iy + 3, Graphics.LEFT | Graphics.TOP);
        }
        return n * lineH + 8;
    }

    boolean contentKey(int keyCode) {
        if (keyCode == Canvas.KEY_NUM2 || keyCode == Canvas.UP) {
            if (cursor > 0) cursor--;
            return true;
        }
        if (keyCode == Canvas.KEY_NUM8 || keyCode == Canvas.DOWN) {
            if (cursor < n - 1) cursor++;
            return true;
        }
        if (keyCode == Canvas.KEY_NUM4 || keyCode == Canvas.LEFT) { scrollY -= 24; return true; }
        if (keyCode == Canvas.KEY_NUM6 || keyCode == Canvas.RIGHT) { scrollY += 24; return true; }
        return false;
    }

    void onFire() { activate(cursor); }

    void activate(int idx) {
        if (idx < 0 || idx >= n) return;
        if (type == MULTIPLE) {
            selected[idx] = !selected[idx];
        } else if (type == IMPLICIT) {
            for (int i = 0; i < n; i++) selected[i] = false;
            selected[idx] = true;
            Command c = selectCommand != null ? selectCommand
                    : (implicitCmd != null ? implicitCmd
                    : (nCommands > 0 ? commands[0] : null));
            if (c != null) fireCommand(c);
        } else {
            setSelectedIndex(idx, true);
        }
        repaintScreen();
    }

    void onSoftLeft() {
        if (nCommands > 0) fireCommand(commands[0]);
        else activate(cursor);
    }

    void onSoftRight() {
        for (int i = 0; i < nCommands; i++)
            if (commands[i].getCommandType() == Command.BACK
                    || commands[i].getCommandType() == Command.EXIT
                    || commands[i].getCommandType() == Command.CANCEL) {
                fireCommand(commands[i]);
                return;
            }
        if (nCommands > 1) fireCommand(commands[1]);
    }

    protected void pointerPressed(int x, int y) {
        int bottom = bufH - 22;
        if (y >= bottom) {
            if (x < bufW / 2) onSoftLeft(); else onSoftRight();
            return;
        }
        Font font = Font.getDefaultFont();
        int lineH = font.getHeight() + 6;
        int idx = (y + scrollY - 24) / lineH;
        if (idx >= 0 && idx < n) { cursor = idx; activate(idx); }
        repaintScreen();
    }

    String softLeftLabel() {
        if (nCommands > 0) return commands[0].getLabel();
        return type == IMPLICIT ? "Select" : null;
    }
    String softRightLabel() { return null; }
}
