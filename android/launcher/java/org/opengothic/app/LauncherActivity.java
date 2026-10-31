package org.opengothic.app;

import android.Manifest;
import android.app.ActivityOptions;
import android.app.ApplicationExitInfo;
import android.annotation.TargetApi;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Typeface;
import android.graphics.Matrix;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputFilter;
import android.text.TextWatcher;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import org.opengothic.launcher.R;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** The library: one screen per game with its Steam art, state and the one action that fits. */
public final class LauncherActivity extends AppCompatActivity {
    private static final int PICK_ARCHIVE = 1;
    private static final int SIGN_IN = 2;
    private static final int BACKUP = 5;
    private static final int RESTORE = 6;
    private static final long REFRESH_INTERVAL = 10 * 60 * 1000;

    private static volatile boolean refreshing;
    private static volatile long refreshed;
    private static volatile String refreshError;
    private static volatile boolean libraryFailed;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;
    private SteamStore store;
    private int selected;
    private boolean importing;
    private boolean launching;
    private Library.Game pending;

    private final View[] covers = new View[Library.GAMES.length];
    private ImageView hero;
    private ImageView logo;
    private TextView title;
    private TextView status;
    private ProgressBar progress;
    private Button primary;
    private Button secondary;
    private Button plain;
    private ImageButton more;
    private View account;
    private TextView accountInitial;
    private TextView accountName;
    private ImageView accountAvatar;
    private String shownAvatar;
    private Button signIn;
    private Button update;
    private Bitmap heroImage;

    private View sheetDim;
    private TextView sheetTitle;
    private TextView sheetSize;
    private RadioGroup languages;
    private Button sheetDownload;
    private OnBackPressedCallback closeSheet;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        setContentView(R.layout.og_launcher);
        prefs = getSharedPreferences("launcher", MODE_PRIVATE);
        store = new SteamStore(getApplicationContext());
        selected = 1;
        for (int i = 0; i < Library.GAMES.length; ++i) {
            if (Library.GAMES[i].folder.equals(prefs.getString("selected", "Gothic2"))) selected = i;
        }

        View root = findViewById(R.id.og_root);
        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            Insets cutout = insets.getInsets(WindowInsetsCompat.Type.displayCutout());
            v.setPadding(cutout.left, cutout.top, cutout.right, cutout.bottom);
            return insets;
        });
        hero = findViewById(R.id.og_hero);
        hero.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> alignHero());
        logo = findViewById(R.id.og_logo);
        title = findViewById(R.id.og_title);
        status = findViewById(R.id.og_status);
        progress = findViewById(R.id.og_progress);
        primary = findViewById(R.id.og_primary);
        secondary = findViewById(R.id.og_secondary);
        plain = findViewById(R.id.og_plain);
        more = findViewById(R.id.og_more);
        more.setOnClickListener(this::showMore);
        account = findViewById(R.id.og_account);
        account.setOnClickListener(this::showAccount);
        accountInitial = findViewById(R.id.og_account_initial);
        accountName = findViewById(R.id.og_account_name);
        accountAvatar = findViewById(R.id.og_account_avatar);
        accountAvatar.setClipToOutline(true);
        signIn = findViewById(R.id.og_sign_in);
        signIn.setOnClickListener(v -> signIn());
        update = findViewById(R.id.og_update);
        ((TextView) findViewById(R.id.og_brand)).setText(getString(R.string.og_brand) + " " + Updates.installedVersion(this));
        update.setOnClickListener(v -> showUpdate());

        int[] ids = {R.id.og_cover0, R.id.og_cover1, R.id.og_cover2};
        for (int i = 0; i < covers.length; ++i) {
            final int index = i;
            final Library.Game game = Library.GAMES[i];
            covers[i] = findViewById(ids[i]);
            covers[i].setContentDescription(game.title);
            covers[i].setOnClickListener(v -> select(index));
            final ImageView image = covers[i].findViewById(R.id.og_cover_image);
            final TextView label = covers[i].findViewById(R.id.og_cover_label);
            image.setClipToOutline(true);
            label.setText(game.name);
            Art.load(this, game.appId, Art.COVER, bitmap -> {
                image.setImageBitmap(bitmap);
                label.setVisibility(View.GONE);
            });
        }

        sheetDim = findViewById(R.id.og_sheet_dim);
        sheetDim.setOnClickListener(v -> closeSheet());
        sheetTitle = findViewById(R.id.og_sheet_title);
        sheetSize = findViewById(R.id.og_sheet_size);
        languages = findViewById(R.id.og_languages);
        languages.setOnCheckedChangeListener((group, id) -> updateSheetSize());
        sheetDownload = findViewById(R.id.og_sheet_download);
        sheetDownload.setOnClickListener(v -> {
            View checked = languages.findViewById(languages.getCheckedRadioButtonId());
            if (checked == null) return;
            closeSheet();
            startDownload(game(), (String) checked.getTag());
        });
        findViewById(R.id.og_sheet_close).setOnClickListener(v -> closeSheet());
        closeSheet = new OnBackPressedCallback(false) {
            @Override
            public void handleOnBackPressed() {
                closeSheet();
            }
        };
        getOnBackPressedDispatcher().addCallback(this, closeSheet);

        showGame();
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        // Portrait only shows behind the feedback dialog, but Play and the menu button should still fit a phone
        final boolean portrait = config.orientation == Configuration.ORIENTATION_PORTRAIT;
        final float dp = getResources().getDisplayMetrics().density;
        View panel = findViewById(R.id.og_panel);
        android.view.ViewGroup.MarginLayoutParams p = (android.view.ViewGroup.MarginLayoutParams) panel.getLayoutParams();
        p.width = Math.round((portrait ? Math.min(460, config.screenWidthDp - 145) : 460) * dp);
        p.setMarginStart(Math.round((portrait ? 20 : 40) * dp));
        panel.setLayoutParams(p);
        primary.setMinWidth(Math.round((portrait ? 120 : 200) * dp));
        primary.setMinimumWidth(primary.getMinWidth());
    }

    @Override
    protected void onResume() {
        super.onResume();
        launching = false;
        findViewById(R.id.og_root).setAlpha(1.f);
        hideSystemBars();
        refreshLibrary(false);
        if (Updates.justUpdated(this)) showWhatsNew();
        Updates.check(this);
        askInstallPermission();
        offerReport();
        handler.post(tick);
    }

    @Override
    protected void onPause() {
        handler.removeCallbacks(tick);
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) hideSystemBars();
    }

    private void hideSystemBars() {
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat bars = WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        bars.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        bars.hide(WindowInsetsCompat.Type.systemBars());
    }

    private final Runnable tick = new Runnable() {
        @Override
        public void run() {
            if (importing && !Importer.running) {
                importing = false;
                if (Importer.error != null) message(Importer.error);
                else if (Importer.imported != null) select(indexOf(Importer.imported));
            }
            File apk = Updates.ready;
            if (apk != null) {
                Updates.ready = null;
                startActivity(Updates.installIntent(LauncherActivity.this, apk));
            }
            if (Updates.error != null) {
                message(Updates.error);
                Updates.error = null;
            }
            String error = refreshError;
            if (error != null) {
                refreshError = null;
                Toast.makeText(LauncherActivity.this, error, Toast.LENGTH_LONG).show();
            }
            render();
            handler.postDelayed(this, 500);
        }
    };

    private Library.Game game() {
        return Library.GAMES[selected];
    }

    private static int indexOf(String folder) {
        for (int i = 0; i < Library.GAMES.length; ++i) {
            if (Library.GAMES[i].folder.equals(folder)) return i;
        }
        return 1;
    }

    private void select(int index) {
        if (index == selected) return;
        selected = index;
        prefs.edit().putString("selected", game().folder).apply();
        showGame();
    }

    /** Art of the selected game; the title stands in until the logo arrives. */
    private void showGame() {
        final Library.Game game = game();
        heroImage = null;
        hero.setImageDrawable(null);
        logo.setImageDrawable(null);
        logo.setVisibility(View.GONE);
        logo.setContentDescription(game.title);
        title.setText(game.title);
        title.setVisibility(View.VISIBLE);
        Art.load(this, game.appId, Art.HERO, bitmap -> {
            if (game != game()) return;
            heroImage = bitmap;
            hero.setImageBitmap(bitmap);
            alignHero();
        });
        Art.load(this, game.appId, Art.LOGO, bitmap -> {
            if (game != game()) return;
            logo.setImageBitmap(bitmap);
            logo.setVisibility(View.VISIBLE);
            title.setVisibility(View.GONE);
        });
        for (int i = 0; i < covers.length; ++i) {
            covers[i].setBackground(i == selected ? ContextCompat.getDrawable(this, R.drawable.og_cover_ring) : null);
            covers[i].setAlpha(i == selected ? 1.f : 0.5f);
            covers[i].setSelected(i == selected);
        }
        render();
    }

    /** Fills the height and keeps the right side, where Steam banners put their characters. */
    private void alignHero() {
        if (heroImage == null || hero.getWidth() == 0 || hero.getHeight() == 0) return;
        float scale = Math.max((float) hero.getWidth() / heroImage.getWidth(), (float) hero.getHeight() / heroImage.getHeight());
        Matrix m = new Matrix();
        m.setScale(scale, scale);
        m.postTranslate(hero.getWidth() - heroImage.getWidth() * scale, (hero.getHeight() - heroImage.getHeight() * scale) / 2);
        hero.setImageMatrix(m);
    }

    private static boolean steamAvailable() {
        return Build.VERSION.SDK_INT >= 26;
    }

    private void render() {
        final Library.Game game = game();
        final File root = Library.root(this);
        final String user = store.name();
        final boolean signedIn = steamAvailable() && user != null;

        account.setVisibility(signedIn ? View.VISIBLE : View.GONE);
        Updates.Release newer = Updates.available(this);
        update.setVisibility(newer != null || Updates.downloading ? View.VISIBLE : View.GONE);
        if (Updates.downloading) setText(update, getString(R.string.og_update_downloading, Updates.percent));
        else if (newer != null) setText(update, getString(R.string.og_update_to, newer.version));
        if (signedIn) {
            setText(accountName, user);
            setText(accountInitial, user.substring(0, 1).toUpperCase(Locale.ROOT));
            showAvatar(store.avatar());
        }

        status.setTextColor(ContextCompat.getColor(this, R.color.og_muted));
        progress.setVisibility(View.GONE);
        more.setVisibility(View.GONE);
        Downloads.State d = Downloads.get(game);

        if (root == null) {
            setText(status, getString(R.string.og_storage_unavailable));
            actions(0, null, 0, null, 0, null);
        } else if (Importer.running) {
            importing = true;
            setText(status, getString(R.string.og_importing, Importer.percent));
            showProgress(Importer.percent / 100.f);
            actions(0, null, 0, null, 0, null);
        } else if (Library.installed(root, game)) {
            setText(status, getString(R.string.og_installed));
            more.setVisibility(View.VISIBLE);
            actions(R.string.og_play, this::play, 0, null, 0, null);
        } else if (d != null && d.active()) {
            if (d.queued) {
                setText(status, getString(R.string.og_queued));
            } else {
                setText(status, d.total > 0 ? getString(R.string.og_downloading_progress, d.describe()) : d.status);
                showProgress(d.total > 0 ? d.fraction() : -1);
            }
            actions(0, null, R.string.og_pause, () -> DownloadService.pause(this, game), R.string.og_cancel, () -> DownloadService.cancel(this, game));
        } else if (d != null && d.error != null) {
            setText(status, d.error);
            status.setTextColor(ContextCompat.getColor(this, R.color.og_error));
            actions(signedIn ? R.string.og_retry : 0, () -> startDownload(game, d.language), 0, null, R.string.og_import_zip, this::pickArchive);
        } else if (Library.staging(root, game).isDirectory()) {
            setText(status, getString(R.string.og_paused));
            actions(signedIn ? R.string.og_resume : 0, () -> startDownload(game, prefs.getString("language_" + game.appId, "")),
                    0, null, R.string.og_cancel, () -> DownloadService.cancel(this, game));
        } else if (signedIn) {
            Set<Integer> owned = store.owned();
            if (owned == null && libraryFailed && !refreshing) {
                setText(status, getString(R.string.og_library_failed));
                status.setTextColor(ContextCompat.getColor(this, R.color.og_error));
                actions(R.string.og_retry, () -> refreshLibrary(true), 0, null, R.string.og_import_zip, this::pickArchive);
            } else if (owned == null) {
                setText(status, getString(R.string.og_checking_library));
                actions(0, null, 0, null, R.string.og_import_zip, this::pickArchive);
            } else if (owned.contains(game.appId) || game.free) {
                setText(status, getString(owned.contains(game.appId) ? R.string.og_in_library : R.string.og_free_on_steam));
                actions(R.string.og_download, this::download, 0, null, R.string.og_import_zip, this::pickArchive);
            } else {
                setText(status, getString(R.string.og_not_owned));
                actions(R.string.og_import_zip, this::pickArchive, R.string.og_view_on_steam, this::openStore, 0, null);
            }
        } else {
            setText(status, getString(steamAvailable() ? R.string.og_not_on_device : R.string.og_not_on_device_no_steam));
            actions(R.string.og_import_zip, this::pickArchive, steamAvailable() ? R.string.og_sign_in_steam : 0, this::signIn, 0, null);
        }
        // Saves outlive the game files, so their backup stays reachable after uninstalling
        if (root != null && !Importer.running && (d == null || !d.active()) && !Backups.files(Library.user(root, game)).isEmpty()) {
            more.setVisibility(View.VISIBLE);
        }
        // The header offers sign-in only where the game itself does not
        boolean cardSignIn = secondary.getVisibility() == View.VISIBLE && getString(R.string.og_sign_in_steam).contentEquals(secondary.getText());
        signIn.setVisibility(steamAvailable() && !signedIn && !cardSignIn ? View.VISIBLE : View.GONE);

        if (sheetDim.getVisibility() == View.VISIBLE && languages.getChildCount() == 0) fillLanguages();
    }

    /** The Steam avatar over the initial, which stays while it loads or when there is none. */
    private void showAvatar(final String url) {
        if (url == null ? shownAvatar == null : url.equals(shownAvatar)) return;
        shownAvatar = url;
        accountAvatar.setVisibility(View.GONE);
        if (url == null) return;
        Art.load(this, "avatar_" + url.substring(url.lastIndexOf('/') + 1), url, bitmap -> {
            if (!url.equals(shownAvatar)) return;
            accountAvatar.setImageBitmap(bitmap);
            accountAvatar.setVisibility(View.VISIBLE);
        });
    }

    private void showProgress(float fraction) {
        progress.setVisibility(View.VISIBLE);
        progress.setIndeterminate(fraction < 0);
        if (fraction >= 0) progress.setProgress(Math.round(fraction * 1000));
    }

    private void actions(int primaryText, Runnable primaryAction, int secondaryText, Runnable secondaryAction, int plainText, Runnable plainAction) {
        action(primary, primaryText, primaryAction);
        action(secondary, secondaryText, secondaryAction);
        action(plain, plainText, plainAction);
    }

    private static void action(Button button, int text, Runnable run) {
        button.setVisibility(text == 0 ? View.GONE : View.VISIBLE);
        if (text == 0) return;
        String label = button.getResources().getString(text);
        if (!label.contentEquals(button.getText())) button.setText(label);
        button.setOnClickListener(v -> run.run());
    }

    private static void setText(TextView view, String text) {
        if (!text.contentEquals(view.getText())) view.setText(text);
    }

    private void play() {
        File root = Library.root(this);
        if (root == null || launching || warnUnsupported(this::play)) return;
        if (!GameFiles.damaged(new File(root, game().folder)).isEmpty()) {
            askRepair(game());
            return;
        }
        // The engine reads the folder name at start, see InstallDetect::androidGame
        try {
            GameFiles.ensureScriptsFolder(new File(root, game().folder));
        } catch (IOException e) {
            message(e.getMessage());
            return;
        }
        try (FileOutputStream out = new FileOutputStream(new File(root, "game.txt"))) {
            out.write((game().folder + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            message(e.getMessage());
            return;
        }
        // Fade to dark and open the game without a slide: its window stays empty until the engine draws the first frame,
        // and whatever shows through until then should be dark rather than the library
        launching = true;
        findViewById(R.id.og_root).animate().alpha(0.f).setDuration(180).withEndAction(() ->
                startActivity(new Intent(this, org.tempest.TempestNativeActivity.class), ActivityOptions.makeCustomAnimation(this, 0, 0).toBundle()))
                .start();
    }

    private void askRepair(Library.Game game) {
        boolean steam = steamAvailable() && store.account() != null;
        AlertDialog.Builder dialog = new AlertDialog.Builder(this)
                .setTitle(getString(R.string.og_damaged_title, game.title))
                .setMessage(steam ? R.string.og_damaged_repair : R.string.og_damaged_reimport)
                .setNegativeButton(R.string.og_cancel, null);
        if (steam) dialog.setPositiveButton(R.string.og_repair, (d, which) -> repair(game));
        dialog.show();
    }

    /** Puts the game back into its download folder: DepotDownloader then checks every file and fetches only the damaged parts. */
    private void repair(Library.Game game) {
        File root = Library.root(this);
        if (root == null) return;
        File staging = Library.staging(root, game);
        if (staging.exists() || !new File(root, game.folder).renameTo(staging)) {
            message(getString(R.string.og_err_move_game));
            return;
        }
        startDownload(game, prefs.getString("language_" + game.appId, "english"));
    }

    private void pickArchive() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*");
        startActivityForResult(intent, PICK_ARCHIVE);
    }

    private void openStore() {
        startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://store.steampowered.com/app/" + game().appId + "/")));
    }

    private void signIn() {
        if (steamAvailable()) startActivityForResult(new Intent(this, SignInActivity.class), SIGN_IN);
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == PICK_ARCHIVE && result == RESULT_OK && data != null && data.getData() != null) {
            Importer.start(this, data.getData());
            importing = true;
        } else if (request == SIGN_IN && result == RESULT_OK) {
            refreshed = SystemClock.uptimeMillis();
        } else if (request == BACKUP && result == RESULT_OK && data != null && data.getData() != null && pending != null) {
            writeBackup(pending, data.getData());
        } else if (request == RESTORE && result == RESULT_OK && data != null && data.getData() != null && pending != null) {
            confirmRestore(pending, data.getData());
        }
        render();
    }

    private void backup() {
        File root = Library.root(this);
        if (root == null || Backups.files(Library.user(root, game())).isEmpty()) {
            Toast.makeText(this, R.string.og_nothing_to_back_up, Toast.LENGTH_LONG).show();
            return;
        }
        pending = game();
        String date = new SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).format(new Date());
        Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("application/zip")
                .putExtra(Intent.EXTRA_TITLE, "OpenGothic " + pending.name + " saves " + date + ".zip");
        startActivityForResult(intent, BACKUP);
    }

    private void restore() {
        pending = game();
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"), RESTORE);
    }

    private void writeBackup(final Library.Game game, final Uri uri) {
        final File root = Library.root(this);
        if (root == null) return;
        new Thread(() -> {
            try (OutputStream out = getContentResolver().openOutputStream(uri)) {
                if (out == null) throw new IOException(getString(R.string.og_err_write_backup));
                int count = Backups.write(Library.user(root, game), game.folder, out);
                handler.post(() -> Toast.makeText(this, getString(R.string.og_backup_done, count), Toast.LENGTH_LONG).show());
            } catch (IOException | RuntimeException e) {
                try {
                    DocumentsContract.deleteDocument(getContentResolver(), uri);
                } catch (Exception ignored) {
                    // The provider keeps the empty file
                }
                handler.post(() -> message(e.getMessage()));
            }
        }, "save-backup").start();
    }

    private void confirmRestore(final Library.Game game, final Uri uri) {
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.og_restore_title, game.title))
                .setMessage(R.string.og_restore_body)
                .setPositiveButton(R.string.og_restore_button, (dialog, which) -> {
                    final File root = Library.root(this);
                    if (root == null) return;
                    new Thread(() -> {
                        try (InputStream in = getContentResolver().openInputStream(uri)) {
                            if (in == null) throw new IOException(getString(R.string.og_err_open_backup));
                            int count = Backups.restore(in, Library.user(root, game), game.folder);
                            handler.post(() -> Toast.makeText(this, getString(R.string.og_restore_done, count), Toast.LENGTH_LONG).show());
                        } catch (Backups.OtherGame e) {
                            Library.Game owner = Library.byFolder(e.folder);
                            handler.post(() -> message(getString(R.string.og_backup_other_game, owner != null ? owner.title : e.folder, game.title)));
                        } catch (IOException | RuntimeException e) {
                            handler.post(() -> message(e.getMessage()));
                        }
                    }, "save-restore").start();
                })
                .setNegativeButton(R.string.og_cancel, null)
                .show();
    }

    private void download() {
        if (warnUnsupported(this::download)) return;
        Map<String, Long> sizes = store.languages(game().appId);
        if (sizes.size() == 1) {
            startDownload(game(), sizes.keySet().iterator().next());
            return;
        }
        // Unknown languages arrive with the library refresh and fill the panel
        if (sizes.isEmpty()) refreshLibrary(true);
        openSheet();
    }

    private void startDownload(Library.Game game, String language) {
        if (!steamAvailable()) return;
        if (Build.VERSION.SDK_INT >= 33 && ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[] {Manifest.permission.POST_NOTIFICATIONS}, 3);
        }
        prefs.edit().putString("language_" + game.appId, language).apply();
        DownloadService.start(this, game, language);
        render();
    }

    private void openSheet() {
        setText(sheetTitle, getString(R.string.og_download_title, game().name));
        languages.removeAllViews();
        fillLanguages();
        sheetDim.setVisibility(View.VISIBLE);
        closeSheet.setEnabled(true);
    }

    private void closeSheet() {
        sheetDim.setVisibility(View.GONE);
        closeSheet.setEnabled(false);
    }

    private void fillLanguages() {
        Map<String, Long> sizes = store.languages(game().appId);
        String preferred = sizes.containsKey(Library.phoneLanguage()) ? Library.phoneLanguage() : "english";
        float density = getResources().getDisplayMetrics().density;
        int checked = View.NO_ID;
        for (String language : sizes.keySet()) {
            RadioButton row = new RadioButton(this);
            row.setId(View.generateViewId());
            row.setTag(language);
            row.setText(Library.languageName(language));
            row.setTextSize(15);
            row.setTextColor(ContextCompat.getColor(this, R.color.og_text));
            row.setTypeface(ResourcesCompat.getFont(this, R.font.og_plex_regular));
            row.setBackgroundResource(R.drawable.og_language_row);
            row.setButtonTintList(ContextCompat.getColorStateList(this, R.color.og_text));
            row.setMinHeight(Math.round(48 * density));
            row.setPadding(Math.round(8 * density), 0, Math.round(12 * density), 0);
            languages.addView(row, new RadioGroup.LayoutParams(RadioGroup.LayoutParams.MATCH_PARENT, RadioGroup.LayoutParams.WRAP_CONTENT));
            if (language.equals(preferred) || checked == View.NO_ID) checked = row.getId();
        }
        if (checked != View.NO_ID) languages.check(checked);
        updateSheetSize();
    }

    private void updateSheetSize() {
        View checked = languages.findViewById(languages.getCheckedRadioButtonId());
        File root = Library.root(this);
        if (checked == null || root == null) {
            setText(sheetSize, getString(R.string.og_checking_languages));
            sheetDownload.setEnabled(false);
            return;
        }
        Long size = store.languages(game().appId).get((String) checked.getTag());
        long needed = size == null ? 0 : size;
        long free = root.getUsableSpace();
        boolean fits = needed < free;
        setText(sheetSize, getString(fits ? R.string.og_needs_space : R.string.og_not_enough_space, Library.size(needed), Library.size(free)));
        sheetSize.setTextColor(ContextCompat.getColor(this, fits ? R.color.og_muted : R.color.og_error));
        sheetDownload.setEnabled(fits);
    }

    private boolean mayInstallUpdates() {
        return Build.VERSION.SDK_INT < 26 || getPackageManager().canRequestPackageInstalls();
    }

    private void openInstallPermission() {
        startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:" + getPackageName())));
    }

    /** Once, on the first start: updates install without a detour when they come. */
    private void askInstallPermission() {
        if (mayInstallUpdates() || prefs.getBoolean("askedInstall", false)) return;
        prefs.edit().putBoolean("askedInstall", true).apply();
        new AlertDialog.Builder(this)
                .setTitle(R.string.og_updates_title)
                .setMessage(R.string.og_updates_body)
                .setPositiveButton(R.string.og_allow, (dialog, which) -> openInstallPermission())
                .setNegativeButton(R.string.og_not_now, null)
                .show();
    }

    /** The notes of the last few releases, kept by hand in res/raw/og_whats_new.txt. */
    private void showWhatsNew() {
        String notes;
        try (InputStream in = getResources().openRawResource(R.raw.og_whats_new)) {
            java.io.ByteArrayOutputStream text = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            for (int n; (n = in.read(buf)) > 0; ) text.write(buf, 0, n);
            notes = text.toString(StandardCharsets.UTF_8.name()).trim();
        } catch (IOException e) {
            return;
        }
        new AlertDialog.Builder(this)
                .setTitle(R.string.og_whats_new)
                .setMessage(notes)
                .setPositiveButton(android.R.string.ok, null)
                .show();
    }

    private void showUpdate() {
        final Updates.Release release = Updates.available(this);
        if (release == null || Updates.downloading) return;
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.og_update_title, release.version))
                .setMessage(release.notes.isEmpty() ? getString(R.string.og_update_notes) : release.notes)
                .setPositiveButton(R.string.og_update, (dialog, which) -> {
                    // Declined on the first start: Android's page once more, then Update again
                    if (!mayInstallUpdates()) {
                        Toast.makeText(this, R.string.og_update_allow, Toast.LENGTH_LONG).show();
                        openInstallPermission();
                        return;
                    }
                    Updates.download(this, release);
                    render();
                })
                .setNegativeButton(R.string.og_cancel, null)
                .show();
    }

    private void showMore(View anchor) {
        File root = Library.root(this);
        if (root == null) return;
        final float dp = getResources().getDisplayMetrics().density;
        final PopupWindow popup = new PopupWindow(this);
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        list.setBackgroundResource(R.drawable.og_menu);
        list.setPadding(0, Math.round(6 * dp), 0, Math.round(6 * dp));

        // Help, your data, contact, then removal on its own
        addMenuRow(list, popup, R.drawable.og_ic_controls, R.string.og_controls, R.color.og_muted, R.color.og_text, this::showControls);
        addMenuRow(list, popup, R.drawable.og_ic_news, R.string.og_whats_new, R.color.og_muted, R.color.og_text, this::showWhatsNew);
        addMenuDivider(list);
        addMenuRow(list, popup, R.drawable.og_ic_backup, R.string.og_backup, R.color.og_muted, R.color.og_text, this::backup);
        addMenuRow(list, popup, R.drawable.og_ic_restore, R.string.og_restore, R.color.og_muted, R.color.og_text, this::restore);
        addMenuDivider(list);
        addMenuRow(list, popup, R.drawable.og_ic_feedback, R.string.og_feedback, R.color.og_muted, R.color.og_text, () -> askReport(game(), null));
        addMenuRow(list, popup, R.drawable.og_ic_heart, R.string.og_donate, R.color.og_accent, R.color.og_text,
                () -> startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse("https://ko-fi.com/solessfir"))));
        if (Library.installed(root, game())) {
            addMenuDivider(list);
            addMenuRow(list, popup, R.drawable.og_ic_delete, R.string.og_uninstall, R.color.og_error, R.color.og_error, this::confirmUninstall);
        }

        popup.setContentView(list);
        popup.setBackgroundDrawable(null);
        popup.setFocusable(true);
        popup.setOutsideTouchable(true);
        popup.setElevation(8 * dp);
        popup.setWidth(LinearLayout.LayoutParams.WRAP_CONTENT);
        popup.setHeight(LinearLayout.LayoutParams.WRAP_CONTENT);
        list.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

        // Beside the button, bottoms lined up, so it stays visible; above it when there is no room on the side (portrait)
        int[] at = new int[2];
        anchor.getLocationInWindow(at);
        final int gap = Math.round(8 * dp);
        final View window = getWindow().getDecorView();
        int x = at[0] + anchor.getWidth() + gap;
        int y = at[1] + anchor.getHeight() - list.getMeasuredHeight();
        if (x + list.getMeasuredWidth() > window.getWidth()) {
            x = at[0];
            y = at[1] - list.getMeasuredHeight() - gap;
        }
        popup.showAtLocation(anchor, android.view.Gravity.TOP | android.view.Gravity.START, x, Math.max(y, gap));
    }

    private void addMenuRow(LinearLayout list, PopupWindow popup, int icon, int text, int iconColor, int textColor, Runnable action) {
        final float dp = getResources().getDisplayMetrics().density;
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(android.view.Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(Math.round(40 * dp));
        row.setPadding(Math.round(16 * dp), 0, Math.round(24 * dp), 0);
        android.util.TypedValue ripple = new android.util.TypedValue();
        getTheme().resolveAttribute(android.R.attr.selectableItemBackground, ripple, true);
        row.setBackgroundResource(ripple.resourceId);

        ImageView image = new ImageView(this);
        image.setImageResource(icon);
        image.setColorFilter(ContextCompat.getColor(this, iconColor));
        LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(Math.round(20 * dp), Math.round(20 * dp));
        size.setMarginEnd(Math.round(14 * dp));
        row.addView(image, size);

        TextView label = new TextView(this);
        label.setText(text);
        label.setTextSize(15);
        label.setTextColor(ContextCompat.getColor(this, textColor));
        label.setTypeface(ResourcesCompat.getFont(this, R.font.og_plex_regular));
        row.addView(label);

        row.setOnClickListener(v -> {
            popup.dismiss();
            action.run();
        });
        list.addView(row, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
    }

    private void addMenuDivider(LinearLayout list) {
        final float dp = getResources().getDisplayMetrics().density;
        View line = new View(this);
        line.setBackgroundColor(ContextCompat.getColor(this, R.color.og_border_soft));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, Math.max(1, Math.round(dp)));
        lp.setMargins(Math.round(16 * dp), Math.round(4 * dp), Math.round(16 * dp), Math.round(4 * dp));
        list.addView(line, lp);
    }

    /** After the game closed by itself, offers to send a report to the developer. */
    private void offerReport() {
        if (Build.VERSION.SDK_INT < 30) return;
        List<ApplicationExitInfo> exits = Reports.gameExits(this);
        if (exits.isEmpty()) return;
        ApplicationExitInfo last = exits.get(0);
        if (last.getTimestamp() <= prefs.getLong("reported_exit", 0)) return;
        prefs.edit().putLong("reported_exit", last.getTimestamp()).apply();
        File root = Library.root(this);
        final Library.Game game = root == null ? null : Reports.lastPlayed(root);
        int problem = Reports.problem(last);
        if (game == null || problem == 0) return;
        askReport(game, getString(problem, game.name));
    }

    /** The touch layout and the gestures the screen does not show. */
    private void showControls() {
        final float density = getResources().getDisplayMetrics().density;
        final int pad = Math.round(20 * density);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(pad, 0, pad, pad);
        ImageView layout = new ImageView(this);
        layout.setImageResource(R.drawable.og_controls);
        layout.setAdjustViewBounds(true);
        layout.setContentDescription(getString(R.string.og_controls));
        page.addView(layout);
        int[][] sections = {
            {R.string.og_controls_move_title, R.string.og_controls_move},
            {R.string.og_controls_buttons_title, R.string.og_controls_buttons},
            {R.string.og_controls_gestures_title, R.string.og_controls_gestures},
            {R.string.og_controls_inventory_title, R.string.og_controls_inventory},
            {R.string.og_controls_screens_title, R.string.og_controls_screens},
        };
        for (int[] section : sections) {
            TextView title = new TextView(this);
            title.setText(section[0]);
            title.setTypeface(Typeface.DEFAULT_BOLD);
            title.setTextSize(16);
            title.setPadding(0, Math.round(16 * density), 0, Math.round(4 * density));
            TextView body = new TextView(this);
            body.setText(section[1]);
            body.setTextSize(14);
            body.setLineSpacing(0, 1.15f);
            page.addView(title);
            page.addView(body);
        }
        ScrollView scroll = new ScrollView(this);
        scroll.addView(page);
        new AlertDialog.Builder(this)
                .setTitle(R.string.og_controls)
                .setView(scroll)
                .setPositiveButton(android.R.string.ok, null)
                .show();
    }

    /**
     * Asks what happened and sends the report. The problem is null when the player opened it from the menu,
     * which also offers sending a suggestion: only its text and the phone summary, without the logs.
     */
    private void askReport(final Library.Game game, final String problem) {
        final EditText note = new EditText(this);
        note.setHint(R.string.og_report_hint);
        note.setFilters(new InputFilter[] {new InputFilter.LengthFilter(2000)});
        final RadioGroup kind = new RadioGroup(this);
        kind.setOrientation(RadioGroup.HORIZONTAL);
        final RadioButton bug = new RadioButton(this);
        bug.setId(View.generateViewId());
        bug.setText(R.string.og_feedback_problem);
        final RadioButton idea = new RadioButton(this);
        idea.setId(View.generateViewId());
        idea.setText(R.string.og_feedback_suggestion);
        RadioGroup.LayoutParams gap = new RadioGroup.LayoutParams(RadioGroup.LayoutParams.WRAP_CONTENT, RadioGroup.LayoutParams.WRAP_CONTENT);
        gap.setMarginEnd(Math.round(24 * getResources().getDisplayMetrics().density));
        kind.addView(bug, gap);
        kind.addView(idea);
        kind.check(bug.getId());
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        if (problem == null) page.addView(kind);
        page.addView(note);
        final AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(problem != null ? problem : getString(R.string.og_feedback))
                .setMessage(R.string.og_report_body)
                .setPositiveButton(R.string.og_report_send, (d, which) ->
                        sendReport(game, problem, note.getText().toString().trim(), kind.getCheckedRadioButtonId() == idea.getId()))
                .setNegativeButton(problem != null ? R.string.og_not_now : R.string.og_cancel, null)
                .create();
        int pad = Math.round(20 * getResources().getDisplayMetrics().density);
        dialog.setView(page, pad, 0, pad, 0);
        // Typing is easier in portrait, so the screen follows the phone until the dialog closes
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);
        dialog.setOnDismissListener(d -> setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE));
        dialog.show();
        // A suggestion needs its text; a problem report is useful from the logs alone.
        final Runnable update = () -> {
            boolean suggestion = kind.getCheckedRadioButtonId() == idea.getId();
            dialog.setMessage(getString(suggestion ? R.string.og_suggestion_body : R.string.og_report_body));
            note.setHint(suggestion ? R.string.og_suggestion_hint : R.string.og_report_hint);
            Button send = dialog.getButton(AlertDialog.BUTTON_POSITIVE);
            send.setText(suggestion ? R.string.og_suggestion_send : R.string.og_report_send);
            send.setEnabled(!suggestion || note.getText().toString().trim().length() > 0);
            // The dialog theme does not dim a disabled button by itself
            send.setAlpha(send.isEnabled() ? 1f : 0.4f);
        };
        kind.setOnCheckedChangeListener((group, id) -> update.run());
        note.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}
            @Override public void afterTextChanged(Editable s) { update.run(); }
        });
    }

    private void sendReport(final Library.Game game, final String problem, final String note, final boolean suggestion) {
        new Thread(() -> {
            try {
                Reports.send(this, game, problem, note, suggestion);
                handler.post(() -> Toast.makeText(this, R.string.og_report_sent, Toast.LENGTH_LONG).show());
            } catch (IOException | RuntimeException e) {
                handler.post(() -> message(getString(R.string.og_report_failed, e.getMessage())));
            }
        }, "problem-report").start();
    }

    /** On a phone known not to run the games, warns once per reason; true while the warning shows. */
    private boolean warnUnsupported(final Runnable then) {
        final String reason = Updates.unsupported(this);
        if (reason == null || reason.equals(prefs.getString("unsupported_ok", null))) return false;
        new AlertDialog.Builder(this)
                .setTitle(R.string.og_unsupported_title)
                .setMessage(reason)
                .setPositiveButton(R.string.og_try_anyway, (d, which) -> {
                    prefs.edit().putString("unsupported_ok", reason).apply();
                    then.run();
                })
                .setNegativeButton(R.string.og_cancel, null)
                .show();
        return true;
    }

    private void confirmUninstall() {
        final Library.Game game = game();
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.og_uninstall_title, game.title))
                .setMessage(R.string.og_uninstall_body)
                .setPositiveButton(R.string.og_uninstall, (dialog, which) -> new Thread(() -> {
                    File root = Library.root(this);
                    try {
                        if (root != null) GameFiles.delete(new File(root, game.folder));
                    } catch (IOException e) {
                        handler.post(() -> message(e.getMessage()));
                    }
                }, "game-uninstall").start())
                .setNegativeButton(R.string.og_cancel, null)
                .show();
    }

    private void showAccount(View anchor) {
        PopupMenu menu = new PopupMenu(this, anchor);
        menu.getMenu().add(R.string.og_sign_out);
        menu.setOnMenuItemClickListener(item -> {
            store.clear();
            render();
            return true;
        });
        menu.show();
    }

    private void message(String text) {
        new AlertDialog.Builder(this).setMessage(text).setPositiveButton(android.R.string.ok, null).show();
    }

    /** Reads which games the account owns, at most every few minutes; the cached answer shows meanwhile. */
    private void refreshLibrary(boolean force) {
        if (!steamAvailable() || store.account() == null || refreshing) return;
        if (!force && store.owned() != null && refreshed != 0 && SystemClock.uptimeMillis() - refreshed < REFRESH_INTERVAL) return;
        refreshing = true;
        final SteamStore steamStore = store;
        final String ended = getString(R.string.og_session_ended);
        new Thread(() -> readLibrary(steamStore, ended), "steam-library").start();
    }

    @TargetApi(26)
    private static void readLibrary(SteamStore store, String ended) {
        SteamStore.Session saved = store.load();
        SteamSession steam = saved == null ? null : new SteamSession();
        boolean done = false;
        try {
            if (steam != null) {
                steam.connect();
                steam.resume(saved);
                steam.readLibrary(store);
            }
            done = true;
        } catch (SteamSession.Failure e) {
            // Offline keeps the cached library; only an ended session signs out
            Log.w("OpenGothic/Steam", "library refresh: " + e.getMessage());
            if (e.signIn) {
                store.clear();
                refreshError = ended;
                done = true;
            }
        } finally {
            if (steam != null) steam.close();
            libraryFailed = !done;
            refreshed = SystemClock.uptimeMillis();
            refreshing = false;
        }
    }
}
