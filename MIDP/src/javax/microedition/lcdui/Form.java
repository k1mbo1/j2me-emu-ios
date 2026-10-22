package javax.microedition.lcdui;

import java.util.Vector;

public class Form extends Screen {

    final Vector items = new Vector();
    Item focus;          /* 包内：TextField 光标渲染依赖 */
    int focusIdx = -1;

    public Form(String title) { super(title); }

    public int append(Item item) {
        if (item == null) throw new NullPointerException();
        item.owner = this;
        items.addElement(item);
        if (focusIdx < 0) focusIdx = items.size() - 1;
        focus = item;
        repaintScreen();
        return items.size() - 1;
    }

    public int append(String str) { return append(new StringItem(null, str)); }

    public void insert(int itemNum, Item item) {
        if (itemNum < 0 || itemNum > items.size()) throw new IndexOutOfBoundsException();
        if (item == null) throw new NullPointerException();
        item.owner = this;
        items.insertElementAt(item, itemNum);
        if (focusIdx >= itemNum) focusIdx++;
        repaintScreen();
    }

    public void delete(int itemNum) {
        if (itemNum < 0 || itemNum >= items.size()) throw new IndexOutOfBoundsException();
        items.removeElementAt(itemNum);
        if (focusIdx >= items.size()) focusIdx = items.size() - 1;
        if (focusIdx == itemNum - 1 || focusIdx < 0) focusIdx = 0;
        focus = focusIdx >= 0 && focusIdx < items.size() ? (Item) items.elementAt(focusIdx) : null;
        repaintScreen();
    }

    public void deleteAll() {
        items.removeAllElements();
        focusIdx = -1;
        focus = null;
        repaintScreen();
    }

    public void set(int itemNum, Item item) {
        if (itemNum < 0 || itemNum >= items.size()) throw new IndexOutOfBoundsException();
        if (item == null) throw new NullPointerException();
        item.owner = this;
        items.setElementAt(item, itemNum);
        if (focusIdx == itemNum) focus = item;
        repaintScreen();
    }

    public Item get(int itemNum) {
        if (itemNum < 0 || itemNum >= items.size()) throw new IndexOutOfBoundsException();
        return (Item) items.elementAt(itemNum);
    }

    public int size() { return items.size(); }

    int paintContent(Graphics g, int x, int y, int w, int h) {
        Font font = Font.getDefaultFont();
        int cy = 0;
        for (int i = 0; i < items.size(); i++) {
            Item it = (Item) items.elementAt(i);
            int ih = it.contentHeight(this);
            /* 焦点框 */
            if (i == focusIdx) {
                g.setColor(0x3050A0);
                g.drawRect(x + 1, y + cy + 1, w - 4, ih - 2);
            }
            it.paintContent(g, x + 6, y + cy + 2, w - 12, this);
            cy += ih + 4;
        }
        return cy;
    }

    boolean contentKey(int keyCode) {
        if (items.size() == 0) return false;
        Item it = focus;
        if (it instanceof TextField) {
            TextField tf = (TextField) it;
            if (tf.keyInput(keyCode)) return true;
        }
        if (it instanceof ChoiceGroup) {
            ChoiceGroup cg = (ChoiceGroup) it;
            if (cg.keyInput(keyCode)) return true;
        }
        if (keyCode == Canvas.KEY_NUM8 || keyCode == Canvas.DOWN) {
            if (focusIdx < items.size() - 1) { focusIdx++; focus = (Item) items.elementAt(focusIdx); }
            return true;
        }
        if (keyCode == Canvas.KEY_NUM2 || keyCode == Canvas.UP) {
            if (focusIdx > 0) { focusIdx--; focus = (Item) items.elementAt(focusIdx); }
            return true;
        }
        return false;
    }

    void onFire() {
        Item it = focus;
        if (it instanceof TextField) {
            /* 焦点项触发默认命令 */
            it.fireDefault();
        } else if (it != null) {
            it.fireDefault();
        }
    }

    void onSoftLeft() {
        if (nCommands > 0) fireCommand(commands[0]);
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
        else if (nCommands > 0) fireCommand(commands[0]);
    }

    String softLeftLabel() { return nCommands > 0 ? commands[0].getLabel() : null; }
    String softRightLabel() { return "Back"; }
}
