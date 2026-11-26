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
    // The game the shown layout was read from; a game without a file is seeded from it, so it gets what the screen shows
    private final Library.Game from;
    private final float dp;

    private Dialog dialog;
    // Sony's USB vendor; its pads are labelled with symbols instead of letters
    private static final int SONY = 0x054c;
    private boolean playStation;
    private android.content.Context ctx;
    private LinearLayout tabs;
    private TextView[] tabViews;
    private ScrollView tabScroll;
    private TextView hint;
    // One message for a storage problem, not one per game and key
    private boolean writeFailed;
    private LinearLayout rows;
    private int tab;

    // Capture of a new binding: the buttons in the order they went down, and when the first one did
    private String captureKey;
    private TextView captureView;
    private final List<String> captured = new ArrayList<>();
    private final List<String> held = new ArrayList<>();
    private long captureStart;
    private final Runnable captureTimeout = this::cancelCapture;

    private ControllerSetup(Activity activity, File root, Library.Game selected, int lastPad) {
        this.activity = activity;
        this.root = root;
        this.dp = activity.getResources().getDisplayMetrics().density;
        InputDevice used = InputDevice.getDevice(lastPad);
        InputDevice pad = used != null ? used : controller();
        this.playStation = pad != null && pad.getVendorId() == SONY;
        this.defaults = load(activity.getResources().openRawResource(R.raw.og_gamepad_defaults));
        this.current = copy(defaults);
        from = layoutSource(root, selected, null);
        File source = from == null ? null : file(from);
        if (source != null) {
            try {
                // The screen shows what the game uses: the engine drops a whole binding section over an unknown action,
                // an invalid value or a clash, and then plays the defaults for all of it
                for (Map.Entry<String, Map<String, String>> section : load(new FileInputStream(source)).entrySet()) {
                    Map<String, String> into = current.get(section.getKey());
                    if (into == null) continue;
                    if (!Arrays.asList(SECTIONS).contains(section.getKey())) {
                        for (Map.Entry<String, String> e : section.getValue().entrySet())
                            if (into.containsKey(e.getKey())) into.put(e.getKey(), e.getValue());
                        continue;
                    }
                    // Any known action is accepted, also one the section's defaults lack: the engine takes it as an override
                    Map<String, String> loaded = new LinkedHashMap<>(into);
                    boolean accepted = true;
                    for (Map.Entry<String, String> e : section.getValue().entrySet()) {
                        accepted &= isAction(e.getKey()) && valid(e.getValue());
                        loaded.put(e.getKey(), e.getValue());
                    }
                    if (accepted && !hasClash(loaded)) current.put(section.getKey(), loaded);
                }
            } catch (IOException ignored) {
                // An unreadable file shows the defaults
            }
        }
    }

    /** lastPad is the input device id of the gamepad used last in the launcher, or -1. */
    static void show(Activity activity, File root, Library.Game selected, int lastPad) {
        new ControllerSetup(activity, root, selected, lastPad).open();
    }

    private File file(Library.Game game) {
        return file(root, game);
    }

    private static File file(File root, Library.Game game) {
        return new File(Library.user(root, game), "Gamepad.ini");
    }

    /**
     * The game whose Gamepad.ini holds the current layout: the preferred one, then any installed one, then any.
     * An installed game's file is kept up to date; an uninstalled one may be left from long ago.
     */
    private static Library.Game layoutSource(File root, Library.Game preferred, Library.Game exclude) {
        Library.Game any = null;
        if (preferred != null && preferred != exclude && Library.installed(root, preferred) && file(root, preferred).isFile()) return preferred;
        for (Library.Game game : Library.GAMES) {
            if (game == exclude || !file(root, game).isFile()) continue;
            if (Library.installed(root, game)) return game;
            if (any == null) any = game;
        }
        return any;
    }

    /**
     * Before a game first starts, gives it the layout the other games share; with none yet, the engine's commented
     * defaults, the same file the game would write itself, so a hand edit still has the reference to start from.
     */
    static void seed(android.content.Context context, File root, Library.Game game, Library.Game preferred) {
        File target = file(root, game);
        if (target.isFile()) return;
        Library.Game from = layoutSource(root, preferred, game);
        try {
            File dir = target.getParentFile();
            if (dir != null && !dir.isDirectory() && !dir.mkdirs()) return;
            File temp = new File(target.getPath() + ".tmp");
            try {
                try (InputStream in = from != null ? new FileInputStream(file(root, from)) : context.getResources().openRawResource(R.raw.og_gamepad_defaults);
                     java.io.OutputStream out = new FileOutputStream(temp)) {
                    byte[] buffer = new byte[8192];
                    for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
                }
                temp.renameTo(target);
            } finally {
                // Gone after a rename; left only by a failed copy
                temp.delete();
            }
        } catch (IOException ignored) {
            // The game writes its defaults instead
        }
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
            current.get("Axes").put("MovementStick", on ? "RightStick" : "LeftStick");
            current.get("Axes").put("CameraStick", on ? "LeftStick" : "RightStick");
            save("Axes", "MovementStick", "CameraStick");
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
            t.setId(View.generateViewId());
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

        hint = text("", 12, R.color.og_muted);
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
            int scrollY = tabScroll.getScrollY(), height = tabScroll.getHeight();
            if (top < scrollY) tabScroll.smoothScrollTo(0, top);
            else if (bottom > scrollY + height) tabScroll.smoothScrollTo(0, bottom - height);
        });
    }

    /** Rebuilds the actions of the current tab, keeping focus on the given one. */
    private void showRows(String focusKey) {
        rows.removeAllViews();
        final String section = SECTIONS[tab];
        // Also overrides of other actions the file has: they take part in swaps, so they must be visible
        for (String key : current.get(section).keySet()) {
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
        row.setBackgroundResource(R.drawable.og_row);
        row.setFocusable(true);
        // Left goes to the open tab, not the one beside this row, which focusing would select
        row.setNextFocusLeftId(tabViews[tab].getId());
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
                assign(section, key, item.getItemId() == 1 ? "None" : defaultOf(section, key));
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
        followLabels(event.getDevice());
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
            // One tab per press: a held shoulder button would otherwise race through them at key-repeat speed
            case "LB": if (event.getRepeatCount() == 0) selectTab(tab - 1, true); return true;
            case "RB": if (event.getRepeatCount() == 0) selectTab(tab + 1, true); return true;
            case "Menu": dialog.dismiss(); return true;
            case "X":
            case "Y": {
                if (event.getRepeatCount() != 0) return true;
                View focused = dialog.getCurrentFocus();
                if (focused == null || !(focused.getTag() instanceof String)) return true;
                String key = (String) focused.getTag();
                String section = SECTIONS[tab];
                assign(section, key, name.equals("X") ? "None" : defaultOf(section, key));
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
        // A DualShock also lists its motion sensors as a joystick; only a device with gamepad buttons counts
        InputDevice device = event.getDevice();
        if (captureKey == null || device == null || !device.supportsSource(InputDevice.SOURCE_GAMEPAD)
                || !event.isFromSource(InputDevice.SOURCE_JOYSTICK)) return false;
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
        hat(0, hy < -0.5f, "DpadUp");
        hat(1, hy > 0.5f, "DpadDown");
        hat(2, hx < -0.5f, "DpadLeft");
        hat(3, hx > 0.5f, "DpadRight");
        return true;
    }

    /** The labels follow the pad that was used last, as the game does. */
    private void followLabels(InputDevice device) {
        if (!device.supportsSource(InputDevice.SOURCE_GAMEPAD) || (device.getVendorId() == SONY) == playStation) return;
        playStation = !playStation;
        setIcons(hint, names(activity.getString(R.string.og_pad_hint)));
        View focused = dialog.getCurrentFocus();
        if (captureKey == null) showRows(focused != null && focused.getTag() instanceof String ? (String) focused.getTag() : null);
    }

    // Only what the hat itself pressed: a D-pad that sends key events must not be released by a motion event
    private final boolean[] hatDown = new boolean[4];

    private void hat(int index, boolean down, String name) {
        if (down == hatDown[index]) return;
        hatDown[index] = down;
        if (down) press(name); else release(name);
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
        Arrays.fill(hatDown, false);
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

    /**
     * Gives the action this binding. The engine rejects duplicates, so another action of the same section loses it,
     * and gets one of this action's old buttons only if it would otherwise have none.
     */
    private void assign(String section, String key, String binding) {
        List<String> old = choices(value(section, key));
        List<String> wanted = choices(binding);
        for (String other : current.get(section).keySet()) {
            if (other.equals(key)) continue;
            List<String> kept = choices(value(section, other));
            if (!kept.removeIf(c -> clashes(c, wanted))) continue;
            boolean handed = false;
            // The other action keeps its remaining alternatives; one left with none gets one of this action's old buttons
            // that the new binding does not use; removing it from old keeps a second action from getting it too
            for (java.util.Iterator<String> it = old.iterator(); kept.isEmpty() && it.hasNext(); ) {
                String choice = it.next();
                if (clashes(choice, wanted)) continue;
                kept.add(choice);
                it.remove();
                handed = true;
            }
            current.get(section).put(other, kept.isEmpty() ? "None" : String.join(",", kept));
            // Only news worth a message: the old button moved over, or the action is left without any
            if (handed || kept.isEmpty())
                Toast.makeText(activity, activity.getString(handed ? R.string.og_pad_swapped : R.string.og_pad_unbound_other, label(other)),
                        Toast.LENGTH_SHORT).show();
        }
        current.get(section).put(key, binding);
        save(section);
        showRows(key);
    }

    private static List<String> choices(String value) {
        List<String> out = new ArrayList<>();
        for (String c : value.split(",")) {
            c = c.trim();
            if (!c.isEmpty() && !c.equals("None")) out.add(c);
        }
        return out;
    }

    private static boolean clashes(String choice, List<String> others) {
        for (String o : others)
            if (same(choice, o)) return true;
        return false;
    }

    /** Whether two bindings conflict for the engine: the same set of buttons, in any order, and the same hold. */
    private static boolean same(String a, String b) {
        boolean holdA = a.startsWith("Hold:"), holdB = b.startsWith("Hold:");
        return holdA == holdB && buttons(holdA ? a.substring(5) : a).equals(buttons(holdB ? b.substring(5) : b));
    }

    private static java.util.Set<String> buttons(String chord) {
        java.util.Set<String> out = new java.util.HashSet<>();
        for (String b : chord.split("\\+")) {
            b = b.trim();
            // The engine reads Start and Select as Menu and View
            out.add(b.equals("Start") ? "Menu" : b.equals("Select") ? "View" : b);
        }
        return out;
    }

    private String value(String section, String key) {
        Map<String, String> values = current.get(section);
        String v = values == null ? null : values.get(key);
        return v == null ? "None" : v;
    }

    /**
     * Writes a section from current to every game, once all of a change is in current, so a file never holds half of it.
     * A binding section goes whole, so the files cannot drift apart into conflicting bindings; other sections hold tuning
     * values the screen does not show, so only the given keys are written there.
     */
    private void save(String section, String... keys) {
        Map<String, String> values = new LinkedHashMap<>();
        boolean whole = Arrays.asList(SECTIONS).contains(section);
        if (whole) values.putAll(current.get(section));
        else for (String key : keys) values.put(key, current.get(section).get(key));
        // An uninstalled game with a file left over is kept current too; one without gets the layout from seed() when it first starts
        for (Library.Game game : Library.GAMES) {
            if (!Library.installed(root, game) && !file(game).isFile()) continue;
            try {
                // A game not started yet starts from the shared layout, not from this one section
                seed(activity, root, game, from);
                write(file(game), section, values, whole);
            } catch (IOException e) {
                if (!writeFailed) Toast.makeText(activity, e.getMessage(), Toast.LENGTH_LONG).show();
                writeFailed = true;
            }
        }
    }

    /** Puts the bindings and the stick choice back to the defaults; hand-tuned values in other sections stay. */
    private void resetAll() {
        new FocusDialog(ctx)
                .setMessage(R.string.og_pad_reset_confirm)
                .setPositiveButton(R.string.og_pad_reset_all, (d, which) -> {
                    for (String section : SECTIONS) {
                        current.put(section, new LinkedHashMap<>(defaults.get(section)));
                        save(section);
                    }
                    current.get("Axes").put("MovementStick", defaults.get("Axes").get("MovementStick"));
                    current.get("Axes").put("CameraStick", defaults.get("Axes").get("CameraStick"));
                    save("Axes", "MovementStick", "CameraStick");
                    dialog.dismiss();
                    open();
                })
                .setNegativeButton(R.string.og_cancel, null)
                .show();
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
            {"LB", "L1"}, {"RB", "R1"}, {"LT", "L2"}, {"RT", "R2"}, {"Menu", "Options"}, {"View", "Share"},
            {"Start", "Options"}, {"Select", "Share"}};
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

    private static final java.util.regex.Pattern WORD = java.util.regex.Pattern.compile("[A-Za-z0-9]+");

    /** Gamepad.ini keeps Xbox names; a Sony pad shows its own labels. Whole words only, so DpadUp or LeftStickUp stay. */
    private String names(String text) {
        if (!playStation) return text;
        java.util.regex.Matcher m = WORD.matcher(text);
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

    private static final java.util.Set<String> BUTTONS = new java.util.HashSet<>(Arrays.asList(
            "A", "B", "X", "Y", "LB", "RB", "L3", "R3", "Menu", "View", "Start", "Select", "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
            "LT", "RT", "LeftStickUp", "LeftStickDown", "LeftStickLeft", "LeftStickRight",
            "RightStickUp", "RightStickDown", "RightStickLeft", "RightStickRight"));

    /** Whether the engine accepts the value: None, or alternatives of known buttons, each used once per chord. */
    private static boolean valid(String value) {
        if (value.equals("None")) return true;
        if (value.trim().isEmpty()) return false;
        for (String choice : value.split(",")) {
            String c = choice.trim();
            if (c.startsWith("Hold:")) c = c.substring(5);
            if (c.isEmpty() || c.endsWith("+")) return false;
            java.util.Set<String> seen = new java.util.HashSet<>();
            for (String b : c.split("\\+"))
                if (!BUTTONS.contains(b.trim()) || !seen.add(buttons(b).iterator().next())) return false;
        }
        return true;
    }

    private static boolean hasClash(Map<String, String> section) {
        List<String> all = new ArrayList<>();
        for (String value : section.values()) {
            for (String choice : choices(value)) {
                if (clashes(choice, all)) return true;
                all.add(choice);
            }
        }
        return false;
    }

    /** An override the section's defaults lack defaults to None, which in an override section falls back to the base binding. */
    private String defaultOf(String section, String key) {
        String value = defaults.get(section).get(key);
        return value == null ? "None" : value;
    }

    private static boolean isAction(String key) {
        for (Object[] action : ACTIONS)
            if (action[0].equals(key)) return true;
        return false;
    }

    private String label(String key) {
        for (Object[] action : ACTIONS)
            if (action[0].equals(key)) return activity.getString((Integer) action[1]);
        return key;
    }

    /** A real pad first; a virtual one, such as a remapping app or Remote Play, also plays the games. */
    private static InputDevice controller() {
        InputDevice virtual = null;
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice device = InputDevice.getDevice(id);
            if (device == null || !device.supportsSource(InputDevice.SOURCE_GAMEPAD)) continue;
            if (!device.isVirtual()) return device;
            if (virtual == null) virtual = device;
        }
        return virtual;
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
                    // A section listed twice adds up, as in the engine
                    section = out.computeIfAbsent(line.substring(1, line.length() - 1), k -> new LinkedHashMap<>());
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

    /**
     * Sets the section's keys, keeping the rest of the file and its comments, also those after a value.
     * With whole, the section holds exactly these keys: any other one is commented out.
     */
    private static void write(File file, String section, Map<String, String> values, boolean whole) throws IOException {
        List<String> lines = new ArrayList<>();
        if (file.isFile()) {
            try (BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
                String line;
                while ((line = in.readLine()) != null) lines.add(line);
            }
        }
        // A section listed twice adds up in the engine, so keys are replaced in every copy; new ones go to the first
        int sectionAt = -1, end = lines.size();
        boolean inside = false;
        Map<String, String> missing = new LinkedHashMap<>(values);
        for (int i = 0; i < lines.size(); ++i) {
            String l = strip(lines.get(i));
            if (l.startsWith("[") && l.endsWith("]")) {
                if (inside && end == lines.size()) end = i;
                inside = l.substring(1, l.length() - 1).equals(section);
                if (inside && sectionAt < 0) sectionAt = i;
            } else if (inside && l.contains("=")) {
                String key = l.substring(0, l.indexOf('=')).trim();
                if (values.containsKey(key)) {
                    int comment = lines.get(i).indexOf(';');
                    lines.set(i, key + "=" + values.get(key) + (comment >= 0 ? " " + lines.get(i).substring(comment) : ""));
                    missing.remove(key);
                } else if (whole) {
                    // Not in the shared layout, such as an unknown name the engine would drop the whole section over; as a comment it is ignored
                    lines.set(i, "; " + lines.get(i));
                }
            }
        }
        List<String> added = new ArrayList<>();
        for (Map.Entry<String, String> e : missing.entrySet()) added.add(e.getKey() + "=" + e.getValue());
        if (sectionAt >= 0) {
            // After the section's last entry, before any blank lines that separate it from the next one
            int at = end;
            while (at > sectionAt + 1 && strip(lines.get(at - 1)).isEmpty()) --at;
            lines.addAll(at, added);
        } else {
            if (!lines.isEmpty() && !lines.get(lines.size() - 1).trim().isEmpty()) lines.add("");
            lines.add("[" + section + "]");
            lines.addAll(added);
        }
        File dir = file.getParentFile();
        if (dir != null && !dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create " + dir);
        File temp = new File(file.getPath() + ".tmp");
        try {
            try (Writer out = new OutputStreamWriter(new FileOutputStream(temp), StandardCharsets.UTF_8)) {
                for (String l : lines) out.write(l + "\n");
            }
            if (!temp.renameTo(file)) throw new IOException("Cannot write " + file);
        } finally {
            // Gone after a rename; left only by a failed write
            temp.delete();
        }
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
        b.setBackgroundResource(R.drawable.og_row);
        b.setOnClickListener(click);
        LauncherActivity.addFocusRings(b);
        return b;
    }
}
