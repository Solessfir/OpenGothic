package org.opengothic.app;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.content.ContextCompat;

import org.opengothic.launcher.R;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Rebinds controller buttons by editing each game's Gamepad.ini. Every game gets the same layout, and the engine
 * reads the file when a game starts. Keys a file does not have keep the engine defaults.
 */
final class ControllerSetup {
    /** Sections of Gamepad.ini the screen shows, as tabs. Combat and Inventory tabs override Exploration and Menus. */
    private static final String[] SECTIONS = {"Gameplay", "ModernMelee", "ClassicMelee", "Ranged", "UI", "Inventory", "EquipmentWheel", "Interaction"};
    private static final int[] SECTION_NAMES = {R.string.og_pad_tab_gameplay, R.string.og_pad_tab_modern, R.string.og_pad_tab_classic,
            R.string.og_pad_tab_ranged, R.string.og_pad_tab_menus, R.string.og_pad_tab_inventory, R.string.og_pad_tab_wheel,
            R.string.og_pad_tab_interaction};

    private static final Object[][] ACTIONS = {
        {"Interact", R.string.og_pad_interact}, {"Back", R.string.og_pad_back}, {"Jump", R.string.og_pad_jump},
        {"Journal", R.string.og_pad_journal}, {"Inventory", R.string.og_pad_inventory}, {"Pause", R.string.og_pad_pause},
        {"SystemWheel", R.string.og_pad_system_wheel}, {"Sneak", R.string.og_pad_sneak}, {"AutoRun", R.string.og_pad_auto_run},
        {"Walk", R.string.og_pad_walk}, {"LockTarget", R.string.og_pad_lock_target}, {"DrawSheathe", R.string.og_pad_draw},
        {"EquipmentWheel", R.string.og_pad_equipment_wheel}, {"Map", R.string.og_pad_map},
        {"HealthPotion", R.string.og_pad_health_potion}, {"ManaPotion", R.string.og_pad_mana_potion},
        {"QuickUp", R.string.og_pad_quick_up}, {"QuickDown", R.string.og_pad_quick_down},
        {"QuickLeft", R.string.og_pad_quick_left}, {"QuickRight", R.string.og_pad_quick_right},
        {"WheelUp", R.string.og_pad_wheel_up}, {"WheelDown", R.string.og_pad_wheel_down},
        {"WheelLeft", R.string.og_pad_wheel_left}, {"WheelRight", R.string.og_pad_wheel_right},
        {"CharacterStats", R.string.og_pad_stats}, {"FirstPerson", R.string.og_pad_first_person},
        {"LookBehind", R.string.og_pad_look_behind}, {"QuickSave", R.string.og_pad_quicksave}, {"QuickLoad", R.string.og_pad_quickload},
        {"AttackForward", R.string.og_pad_attack}, {"AttackLeft", R.string.og_pad_attack_left},
        {"AttackRight", R.string.og_pad_attack_right}, {"Block", R.string.og_pad_block}, {"Finish", R.string.og_pad_finish},
        {"Accept", R.string.og_pad_accept}, {"DeleteSave", R.string.og_pad_delete_save}, {"RenameSave", R.string.og_pad_rename_save},
        {"Up", R.string.og_pad_up}, {"Down", R.string.og_pad_down}, {"Left", R.string.og_pad_left}, {"Right", R.string.og_pad_right},
        {"AdjustLeft", R.string.og_pad_adjust_left}, {"AdjustRight", R.string.og_pad_adjust_right},
        {"PreviousPage", R.string.og_pad_previous_page}, {"NextPage", R.string.og_pad_next_page}, {"Cancel", R.string.og_pad_cancel},
        {"AssignUp", R.string.og_pad_assign_up}, {"AssignDown", R.string.og_pad_assign_down},
        {"AssignLeft", R.string.og_pad_assign_left}, {"AssignRight", R.string.og_pad_assign_right},
        {"LeftPanel", R.string.og_pad_left_panel}, {"RightPanel", R.string.og_pad_right_panel},
        {"TakeStack", R.string.og_pad_take_stack}, {"Drop", R.string.og_pad_drop},
        {"Spell3", R.string.og_pad_spell3}, {"Spell4", R.string.og_pad_spell4}, {"Spell5", R.string.og_pad_spell5},
        {"Spell6", R.string.og_pad_spell6}, {"Spell7", R.string.og_pad_spell7}, {"Spell8", R.string.og_pad_spell8},
        {"Spell9", R.string.og_pad_spell9}, {"Spell10", R.string.og_pad_spell10},
    };

    // Longer than the engine's 400 ms hold, so a normal press is not taken for a hold by accident
    private static final long HOLD_MS = 700;
    private static final long CAPTURE_TIMEOUT_MS = 6000;

    private final Activity activity;
    private final File root;
    private final Map<String, Map<String, String>> defaults;
    private final Map<String, Map<String, String>> current;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final float dp;

    private Dialog dialog;
    // Sony's USB vendor; its pads are labelled with symbols instead of letters
    private final boolean playStation;
    private android.content.Context ctx;
    private LinearLayout tabs;
    private TextView[] tabViews;
    private ScrollView tabScroll;
    private LinearLayout rows;
    private int tab;

    // Capture of a new binding: the buttons in the order they went down, and when the first one did
    private String captureKey;
    private TextView captureView;
    private final List<String> captured = new ArrayList<>();
    private final List<String> held = new ArrayList<>();
    private long captureStart;
    private final Runnable captureTimeout = this::cancelCapture;

    private ControllerSetup(Activity activity, File root, Library.Game selected) {
        this.activity = activity;
        this.root = root;
        this.dp = activity.getResources().getDisplayMetrics().density;
        InputDevice pad = controller();
        this.playStation = pad != null && pad.getVendorId() == 0x054c;
        this.defaults = load(activity.getResources().openRawResource(R.raw.og_gamepad_defaults));
        this.current = copy(defaults);
        File source = file(selected);
        if (!source.isFile()) {
            for (Library.Game game : Library.GAMES) {
                if (file(game).isFile()) {
                    source = file(game);
                    break;
                }
            }
        }
        if (source.isFile()) {
            try {
                for (Map.Entry<String, Map<String, String>> section : load(new FileInputStream(source)).entrySet()) {
                    Map<String, String> into = current.get(section.getKey());
                    if (into != null) into.putAll(section.getValue());
                }
            } catch (IOException ignored) {
                // An unreadable file shows the defaults
            }
        }
    }

    static void show(Activity activity, File root, Library.Game selected) {
        new ControllerSetup(activity, root, selected).open();
    }

    private File file(Library.Game game) {
        return new File(Library.user(root, game), "Gamepad.ini");
    }

    private void open() {
        dialog = new Dialog(activity, R.style.Launcher_Dialog) {
            @Override
            public boolean dispatchKeyEvent(KeyEvent event) {
                return onKey(event) || super.dispatchKeyEvent(event);
            }

            @Override
            public boolean dispatchGenericMotionEvent(MotionEvent event) {
                return onMotion(event) || super.dispatchGenericMotionEvent(event);
            }

            @Override
            public boolean dispatchTouchEvent(MotionEvent event) {
                // Touching the screen cancels a capture, so every controller button stays bindable
                if (captureKey != null) {
                    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) cancelCapture();
                    return true;
                }
                return super.dispatchTouchEvent(event);
            }
        };
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        ctx = dialog.getContext();

        int pad = Math.round(20 * dp);
        LinearLayout page = new LinearLayout(ctx);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(pad, pad, pad, Math.round(12 * dp));

        // Title and controller on the left, the buttons on the right so they never need scrolling to
        LinearLayout header = new LinearLayout(ctx);
        header.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout heading = new LinearLayout(ctx);
        heading.setOrientation(LinearLayout.VERTICAL);
        TextView title = text(activity.getString(R.string.og_pad_title), 20, R.color.og_text);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        heading.addView(title);
        String name = controllerName();
        TextView device = text(name == null ? activity.getString(R.string.og_pad_none) : activity.getString(R.string.og_pad_connected, name), 13, R.color.og_muted);
        device.setPadding(0, Math.round(4 * dp), 0, 0);
        heading.addView(device);
        header.addView(heading, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        header.addView(button(activity.getString(R.string.og_pad_reset_all), v -> resetAll()));
        header.addView(button(activity.getString(android.R.string.ok), v -> dialog.dismiss()));
        page.addView(header);

        Switch swap = new Switch(ctx);
        swap.setText(R.string.og_pad_swap_sticks);
        swap.setTextColor(ContextCompat.getColor(activity, R.color.og_text));
        swap.setChecked("RightStick".equals(value("Axes", "MovementStick")));
        swap.setOnCheckedChangeListener((v, on) -> {
            set("Axes", "MovementStick", on ? "RightStick" : "LeftStick");
            set("Axes", "CameraStick", on ? "LeftStick" : "RightStick");
        });
        // Same inset as the rows, so the focus ring does not sit on the text
        swap.setPadding(Math.round(8 * dp), Math.round(6 * dp), Math.round(8 * dp), Math.round(6 * dp));
        LauncherActivity.addFocusRings(swap);
        LinearLayout.LayoutParams swapAt = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        swapAt.setMargins(0, Math.round(8 * dp), 0, Math.round(8 * dp));
        page.addView(swap, swapAt);

        // Sections as a column of tabs on the left, the actions of the chosen one on the right
        LinearLayout body = new LinearLayout(ctx);
        tabs = new LinearLayout(ctx);
        tabs.setOrientation(LinearLayout.VERTICAL);
        tabViews = new TextView[SECTIONS.length];
        for (int i = 0; i < SECTIONS.length; ++i) {
            final int at = i;
            TextView t = text(activity.getString(SECTION_NAMES[i]), 15, R.color.og_muted);
            t.setPadding(Math.round(14 * dp), Math.round(8 * dp), Math.round(14 * dp), Math.round(8 * dp));
            t.setFocusable(true);
            t.setOnClickListener(v -> selectTab(at, true));
            // Moving through the tabs with the D-pad shows each one right away
            t.setOnFocusChangeListener((v, focused) -> {
                if (focused && at != tab) selectTab(at, false);
            });
            LauncherActivity.addFocusRings(t);
            tabs.addView(t, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            tabViews[i] = t;
        }
        tabScroll = new ScrollView(ctx);
        tabScroll.addView(tabs);
        body.addView(tabScroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));
        View line = new View(ctx);
        line.setBackgroundColor(ContextCompat.getColor(activity, R.color.og_border_soft));
        LinearLayout.LayoutParams lineAt = new LinearLayout.LayoutParams(Math.max(1, Math.round(dp)), ViewGroup.LayoutParams.MATCH_PARENT);
        lineAt.setMargins(Math.round(8 * dp), 0, Math.round(8 * dp), 0);
        body.addView(line, lineAt);
        rows = new LinearLayout(ctx);
        rows.setOrientation(LinearLayout.VERTICAL);
        ScrollView scroll = new ScrollView(ctx);
        scroll.addView(rows);
        body.addView(scroll, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1));
        page.addView(body, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        TextView hint = text("", 12, R.color.og_muted);
        setIcons(hint, names(activity.getString(R.string.og_pad_hint)));
        hint.setPadding(0, Math.round(8 * dp), 0, 0);
        page.addView(hint);

        dialog.setContentView(page);
        dialog.setOnDismissListener(d -> handler.removeCallbacks(captureTimeout));
        dialog.show();
        if (dialog.getWindow() != null) {
            dialog.getWindow().setLayout(Math.round(Math.min(activity.getResources().getDisplayMetrics().widthPixels - 32 * dp, 820 * dp)),
                    Math.round(activity.getResources().getDisplayMetrics().heightPixels * 0.9f));
        }
        selectTab(0, true);
    }

    private void selectTab(int index, boolean focusRows) {
        tab = (index + SECTIONS.length) % SECTIONS.length;
        for (int i = 0; i < tabViews.length; ++i) {
            boolean on = i == tab;
            tabViews[i].setTextColor(ContextCompat.getColor(activity, on ? R.color.og_accent : R.color.og_muted));
            tabViews[i].setTypeface(on ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
            android.graphics.drawable.GradientDrawable fill = new android.graphics.drawable.GradientDrawable();
            fill.setCornerRadius(8 * dp);
            fill.setColor(on ? ContextCompat.getColor(activity, R.color.og_border_soft) : android.graphics.Color.TRANSPARENT);
            tabViews[i].setBackground(fill);
        }
        showRows(null);
        if (focusRows && rows.getChildCount() > 0) rows.getChildAt(0).requestFocus();
        // Keep the chosen tab in view, also when LB / RB picked it while focus stays in the actions
        final View chosen = tabViews[tab];
        tabScroll.post(() -> {
            int top = chosen.getTop(), bottom = chosen.getBottom();
            int from = tabScroll.getScrollY(), height = tabScroll.getHeight();
            if (top < from) tabScroll.smoothScrollTo(0, top);
            else if (bottom > from + height) tabScroll.smoothScrollTo(0, bottom - height);
        });
    }

    /** Rebuilds the actions of the current tab, keeping focus on the given one. */
    private void showRows(String focusKey) {
        rows.removeAllViews();
        final String section = SECTIONS[tab];
        for (String key : defaults.get(section).keySet()) {
            View row = row(section, key);
            rows.addView(row);
            if (key.equals(focusKey)) row.requestFocus();
        }
    }

    private View row(final String section, final String key) {
        LinearLayout row = new LinearLayout(ctx);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(Math.round(44 * dp));
        row.setPadding(Math.round(8 * dp), 0, Math.round(8 * dp), 0);
        android.util.TypedValue ripple = new android.util.TypedValue();
        ctx.getTheme().resolveAttribute(android.R.attr.selectableItemBackground, ripple, true);
        row.setBackgroundResource(ripple.resourceId);
        row.setFocusable(true);
        LauncherActivity.addFocusRings(row);
        row.setTag(key);

        TextView label = text(label(key), 15, R.color.og_text);
        row.addView(label, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        final TextView binding = text("", 14, R.color.og_muted);
        setIcons(binding, display(value(section, key)));
        binding.setGravity(Gravity.END);
        row.addView(binding);

        row.setOnClickListener(v -> startCapture(key, binding));
        row.setOnLongClickListener(v -> {
            PopupMenu menu = new PopupMenu(ctx, v);
            menu.getMenu().add(0, 1, 0, R.string.og_pad_clear);
            menu.getMenu().add(0, 2, 0, R.string.og_pad_default);
            menu.setOnMenuItemClickListener(item -> {
                assign(section, key, item.getItemId() == 1 ? "None" : defaults.get(section).get(key));
                return true;
            });
            menu.show();
            return true;
        });
        return row;
    }

    // ---- Input ----

    private static boolean fromController(InputDevice device) {
        return device != null && (device.supportsSource(InputDevice.SOURCE_GAMEPAD) || device.supportsSource(InputDevice.SOURCE_JOYSTICK));
    }

    private static String buttonName(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_BUTTON_A: return "A";
            case KeyEvent.KEYCODE_BUTTON_B: return "B";
            case KeyEvent.KEYCODE_BUTTON_X: return "X";
            case KeyEvent.KEYCODE_BUTTON_Y: return "Y";
            case KeyEvent.KEYCODE_BUTTON_L1: return "LB";
            case KeyEvent.KEYCODE_BUTTON_R1: return "RB";
            case KeyEvent.KEYCODE_BUTTON_L2: return "LT";
            case KeyEvent.KEYCODE_BUTTON_R2: return "RT";
            case KeyEvent.KEYCODE_BUTTON_THUMBL: return "L3";
            case KeyEvent.KEYCODE_BUTTON_THUMBR: return "R3";
            case KeyEvent.KEYCODE_BUTTON_START: return "Menu";
            case KeyEvent.KEYCODE_BUTTON_SELECT: return "View";
            case KeyEvent.KEYCODE_DPAD_UP: return "DpadUp";
            case KeyEvent.KEYCODE_DPAD_DOWN: return "DpadDown";
            case KeyEvent.KEYCODE_DPAD_LEFT: return "DpadLeft";
            case KeyEvent.KEYCODE_DPAD_RIGHT: return "DpadRight";
            default: return null;
        }
    }

    private boolean onKey(KeyEvent event) {
        if (!fromController(event.getDevice())) return false;
        String name = buttonName(event.getKeyCode());
        if (captureKey != null) {
            if (name != null && event.getRepeatCount() == 0) {
                if (event.getAction() == KeyEvent.ACTION_DOWN) press(name);
                else if (event.getAction() == KeyEvent.ACTION_UP) release(name);
            }
            return true;
        }
        if (event.getAction() != KeyEvent.ACTION_DOWN || name == null) return false;
        switch (name) {
            case "LB": selectTab(tab - 1, true); return true;
            case "RB": selectTab(tab + 1, true); return true;
            case "Menu": dialog.dismiss(); return true;
            case "X":
            case "Y": {
                View focused = dialog.getCurrentFocus();
                if (focused == null || !(focused.getTag() instanceof String)) return true;
                String key = (String) focused.getTag();
                String section = SECTIONS[tab];
                assign(section, key, name.equals("X") ? "None" : defaults.get(section).get(key));
                return true;
            }
            default: return false;
        }
    }

    // Stick directions and analog triggers count as buttons, as the engine names them
    private final boolean[] axisDown = new boolean[10];
    private static final String[] AXIS_NAMES = {"LeftStickUp", "LeftStickDown", "LeftStickLeft", "LeftStickRight",
            "RightStickUp", "RightStickDown", "RightStickLeft", "RightStickRight", "LT", "RT"};

    private boolean onMotion(MotionEvent event) {
        if (captureKey == null || !fromController(event.getDevice())) return false;
        float lx = event.getAxisValue(MotionEvent.AXIS_X), ly = event.getAxisValue(MotionEvent.AXIS_Y);
        float rx = axis(event, MotionEvent.AXIS_Z, MotionEvent.AXIS_RX), ry = axis(event, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RY);
        float lt = Math.max(event.getAxisValue(MotionEvent.AXIS_LTRIGGER), event.getAxisValue(MotionEvent.AXIS_BRAKE));
        float rt = Math.max(event.getAxisValue(MotionEvent.AXIS_RTRIGGER), event.getAxisValue(MotionEvent.AXIS_GAS));
        float hx = event.getAxisValue(MotionEvent.AXIS_HAT_X), hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y);
        float[] values = {-ly, ly, -lx, lx, -ry, ry, -rx, rx, lt, rt};
        for (int i = 0; i < values.length; ++i) {
            boolean down = values[i] > (axisDown[i] ? 0.3f : 0.6f);
            if (down != axisDown[i]) {
                axisDown[i] = down;
                if (down) press(AXIS_NAMES[i]); else release(AXIS_NAMES[i]);
            }
        }
        // Some pads send the D-pad as a hat axis only
        hat(hy < -0.5f, "DpadUp");
        hat(hy > 0.5f, "DpadDown");
        hat(hx < -0.5f, "DpadLeft");
        hat(hx > 0.5f, "DpadRight");
        return true;
    }

    private void hat(boolean down, String name) {
        if (down && !held.contains(name)) press(name);
        else if (!down && held.contains(name)) release(name);
    }

    private static float axis(MotionEvent event, int axis, int fallback) {
        InputDevice device = event.getDevice();
        if (device != null && device.getMotionRange(axis, event.getSource()) == null) axis = fallback;
        return event.getAxisValue(axis);
    }

    private void startCapture(String key, TextView view) {
        if (controllerName() == null) {
            Toast.makeText(activity, R.string.og_pad_none, Toast.LENGTH_SHORT).show();
            return;
        }
        cancelCapture();
        captureKey = key;
        captureView = view;
        captured.clear();
        held.clear();
        Arrays.fill(axisDown, false);
        view.setText(R.string.og_pad_press);
        view.setTextColor(ContextCompat.getColor(activity, R.color.og_accent));
        handler.postDelayed(captureTimeout, CAPTURE_TIMEOUT_MS);
    }

    private void press(String name) {
        if (captured.isEmpty()) captureStart = SystemClock.uptimeMillis();
        if (!held.contains(name)) held.add(name);
        if (!captured.contains(name)) captured.add(name);
        handler.removeCallbacks(captureTimeout);
        setIcons(captureView, names(String.join(" + ", captured)));
    }

    private void release(String name) {
        held.remove(name);
        if (!held.isEmpty() || captured.isEmpty()) return;
        // Modifiers go first, as the engine expects; a single button held long becomes a hold binding
        String binding = String.join("+", captured);
        if (captured.size() == 1 && SystemClock.uptimeMillis() - captureStart >= HOLD_MS) binding = "Hold:" + binding;
        String key = captureKey;
        captureKey = null;
        assign(SECTIONS[tab], key, binding);
    }

    private void cancelCapture() {
        handler.removeCallbacks(captureTimeout);
        if (captureKey == null) return;
        String key = captureKey;
        captureKey = null;
        showRows(key);
    }

    // ---- Bindings ----

    /** Gives the action this binding. Another action of the same section that had it takes the old one, since the engine rejects duplicates. */
    private void assign(String section, String key, String binding) {
        String old = value(section, key);
        if (!binding.equals("None")) {
            for (String other : current.get(section).keySet()) {
                if (other.equals(key)) continue;
                List<String> choices = new ArrayList<>(Arrays.asList(value(section, other).split(",")));
                boolean found = false;
                for (int i = 0; i < choices.size(); ++i) {
                    if (same(choices.get(i).trim(), binding)) {
                        choices.remove(i--);
                        found = true;
                    }
                }
                if (!found) continue;
                // The other action keeps its remaining alternatives and gets this action's old binding
                for (String o : old.split(",")) {
                    String choice = o.trim();
                    if (choice.equals("None") || choice.isEmpty()) continue;
                    boolean clash = false;
                    for (String c : choices) clash |= same(c.trim(), choice);
                    if (!clash) choices.add(choice);
                }
                set(section, other, choices.isEmpty() ? "None" : String.join(",", choices));
                Toast.makeText(activity, activity.getString(R.string.og_pad_swapped, label(other)), Toast.LENGTH_SHORT).show();
            }
        }
        set(section, key, binding);
        showRows(key);
    }

    /** Whether two bindings are the same chord: the same buttons, the same last button and the same hold. */
    private static boolean same(String a, String b) {
        boolean holdA = a.startsWith("Hold:"), holdB = b.startsWith("Hold:");
        if (holdA != holdB) return false;
        List<String> pa = Arrays.asList((holdA ? a.substring(5) : a).split("\\+"));
        List<String> pb = Arrays.asList((holdB ? b.substring(5) : b).split("\\+"));
        return pa.size() == pb.size() && pa.containsAll(pb) && pa.get(pa.size() - 1).equals(pb.get(pb.size() - 1));
    }

    private String value(String section, String key) {
        Map<String, String> values = current.get(section);
        String v = values == null ? null : values.get(key);
        return v == null ? "None" : v;
    }

    private void set(String section, String key, String value) {
        current.get(section).put(key, value);
        for (Library.Game game : Library.GAMES) {
            File dir = Library.user(root, game);
            if (!dir.isDirectory() && !Library.installed(root, game)) continue;
            try {
                write(file(game), section, key, value);
            } catch (IOException e) {
                Toast.makeText(activity, e.getMessage(), Toast.LENGTH_LONG).show();
            }
        }
    }

    private void resetAll() {
        for (Library.Game game : Library.GAMES) {
            File f = file(game);
            if (f.isFile() && !f.delete()) Toast.makeText(activity, f.getPath(), Toast.LENGTH_LONG).show();
        }
        current.clear();
        current.putAll(copy(defaults));
        dialog.dismiss();
        open();
    }

    private String display(String binding) {
        if (binding.equals("None")) return activity.getString(R.string.og_pad_unbound);
        StringBuilder out = new StringBuilder();
        for (String choice : binding.split(",")) {
            String c = choice.trim();
            if (out.length() > 0) out.append(" / ");
            if (c.startsWith("Hold:")) {
                out.append(activity.getString(R.string.og_pad_hold, names(c.substring(5)).replace("+", " + ")));
            } else {
                out.append(names(c).replace("+", " + "));
            }
        }
        return out.toString();
    }

    // The face buttons become private-use characters that icons() draws as matching vector icons:
    // symbol characters come from different fallback fonts and differ a lot in size
    private static final String[][] PLAYSTATION = {{"A", "\uE000"}, {"B", "\uE001"}, {"X", "\uE002"}, {"Y", "\uE003"},
            {"LB", "L1"}, {"RB", "R1"}, {"LT", "L2"}, {"RT", "R2"}, {"Menu", "Options"}, {"View", "Share"}};
    private static final int[] FACE_ICONS = {R.drawable.og_pad_cross, R.drawable.og_pad_circle, R.drawable.og_pad_square, R.drawable.og_pad_triangle};

    /** Sets text whose face button characters show as icons the size of the text, in its color. */
    private void setIcons(TextView view, String text) {
        android.text.SpannableString out = new android.text.SpannableString(text);
        int size = Math.round(view.getTextSize() * 1.1f);
        // A little space after an icon that text follows, so a ":" does not touch it
        int space = Math.round(size * 0.2f);
        int align = android.os.Build.VERSION.SDK_INT >= 29 ? android.text.style.DynamicDrawableSpan.ALIGN_CENTER : android.text.style.DynamicDrawableSpan.ALIGN_BOTTOM;
        for (int i = 0; i < text.length(); ++i) {
            int face = text.charAt(i) - '\uE000';
            if (face < 0 || face >= FACE_ICONS.length) continue;
            int gap = i + 1 < text.length() ? space : 0;
            android.graphics.drawable.Drawable glyph = ContextCompat.getDrawable(ctx, FACE_ICONS[face]).mutate();
            glyph.setTint(view.getCurrentTextColor());
            android.graphics.drawable.Drawable icon = new android.graphics.drawable.InsetDrawable(glyph, 0, 0, gap, 0);
            icon.setBounds(0, 0, size + gap, size);
            out.setSpan(new android.text.style.ImageSpan(icon, align),
                    i, i + 1, android.text.Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
        view.setText(out);
    }

    /** Gamepad.ini keeps Xbox names; a Sony pad shows its own labels. Whole words only, so DpadUp or LeftStickUp stay. */
    private String names(String text) {
        if (!playStation) return text;
        java.util.regex.Matcher m = java.util.regex.Pattern.compile("[A-Za-z0-9]+").matcher(text);
        StringBuffer out = new StringBuffer();
        while (m.find()) {
            String token = m.group();
            for (String[] name : PLAYSTATION)
                if (name[0].equals(token)) token = name[1];
            m.appendReplacement(out, java.util.regex.Matcher.quoteReplacement(token));
        }
        m.appendTail(out);
        return out.toString();
    }

    private String label(String key) {
        for (Object[] action : ACTIONS)
            if (action[0].equals(key)) return activity.getString((Integer) action[1]);
        return key;
    }

    private static InputDevice controller() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device != null && !device.isVirtual() && device.supportsSource(InputDevice.SOURCE_GAMEPAD)) return device;
        }
        return null;
    }

    private static String controllerName() {
        InputDevice device = controller();
        return device == null ? null : device.getName();
    }

    // ---- Gamepad.ini ----

    private static Map<String, Map<String, String>> load(InputStream stream) {
        Map<String, Map<String, String>> out = new LinkedHashMap<>();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            Map<String, String> section = null;
            String line;
            while ((line = in.readLine()) != null) {
                line = strip(line);
                if (line.isEmpty()) continue;
                if (line.startsWith("[") && line.endsWith("]")) {
                    section = new LinkedHashMap<>();
                    out.put(line.substring(1, line.length() - 1), section);
                } else if (section != null && line.contains("=")) {
                    int eq = line.indexOf('=');
                    section.put(line.substring(0, eq).trim(), line.substring(eq + 1).trim());
                }
            }
        } catch (IOException ignored) {
            // What was read so far is used
        }
        return out;
    }

    private static String strip(String line) {
        int comment = line.indexOf(';');
        return (comment >= 0 ? line.substring(0, comment) : line).trim();
    }

    private static Map<String, Map<String, String>> copy(Map<String, Map<String, String>> from) {
        Map<String, Map<String, String>> out = new LinkedHashMap<>();
        for (Map.Entry<String, Map<String, String>> e : from.entrySet()) out.put(e.getKey(), new LinkedHashMap<>(e.getValue()));
        return out;
    }

    /** Sets one key, keeping the rest of the file and its comments. */
    private static void write(File file, String section, String key, String value) throws IOException {
        List<String> lines = new ArrayList<>();
        if (file.isFile()) {
            try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
                String line;
                while ((line = in.readLine()) != null) lines.add(line);
            }
        }
        int sectionAt = -1, end = lines.size(), keyAt = -1;
        for (int i = 0; i < lines.size(); ++i) {
            String l = strip(lines.get(i));
            if (l.startsWith("[") && l.endsWith("]")) {
                if (sectionAt >= 0) {
                    end = i;
                    break;
                }
                if (l.substring(1, l.length() - 1).equals(section)) sectionAt = i;
            } else if (sectionAt >= 0 && l.contains("=") && l.substring(0, l.indexOf('=')).trim().equals(key)) {
                keyAt = i;
            }
        }
        String entry = key + "=" + value;
        if (keyAt >= 0) {
            lines.set(keyAt, entry);
        } else if (sectionAt >= 0) {
            // After the section's last entry, before any blank lines that separate it from the next one
            int at = end;
            while (at > sectionAt + 1 && strip(lines.get(at - 1)).isEmpty()) --at;
            lines.add(at, entry);
        } else {
            if (!lines.isEmpty() && !lines.get(lines.size() - 1).trim().isEmpty()) lines.add("");
            lines.add("[" + section + "]");
            lines.add(entry);
        }
        File dir = file.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create " + dir);
        File temp = new File(file.getPath() + ".tmp");
        try (Writer out = new OutputStreamWriter(new FileOutputStream(temp), StandardCharsets.UTF_8)) {
            for (String l : lines) out.write(l + "\n");
        }
        if (!temp.renameTo(file)) throw new IOException("Cannot write " + file);
    }

    private TextView text(String s, int size, int color) {
        TextView t = new TextView(ctx);
        t.setText(s);
        t.setTextSize(size);
        t.setTextColor(ContextCompat.getColor(activity, color));
        return t;
    }

    private TextView button(String s, View.OnClickListener click) {
        TextView b = text(s, 15, R.color.og_text);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        b.setPadding(Math.round(16 * dp), Math.round(12 * dp), Math.round(16 * dp), Math.round(12 * dp));
        b.setFocusable(true);
        android.util.TypedValue ripple = new android.util.TypedValue();
        ctx.getTheme().resolveAttribute(android.R.attr.selectableItemBackground, ripple, true);
        b.setBackgroundResource(ripple.resourceId);
        b.setOnClickListener(click);
        LauncherActivity.addFocusRings(b);
        return b;
    }
}
