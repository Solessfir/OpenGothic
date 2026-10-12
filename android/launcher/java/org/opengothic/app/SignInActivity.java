package org.opengothic.app;

import android.annotation.TargetApi;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.zxing.BarcodeFormat;
import com.google.zxing.EncodeHintType;
import com.google.zxing.WriterException;
import com.google.zxing.common.BitMatrix;
import com.google.zxing.qrcode.QRCodeWriter;

import org.opengothic.launcher.R;

import java.util.Collections;
import java.util.Locale;
import java.util.concurrent.CompletableFuture;

/** Steam sign-in: password with approval in the Steam app or a Steam Guard code, or a QR code for another device. */
@TargetApi(26)
public final class SignInActivity extends AppCompatActivity {
    private static final String STEAM_APP = "com.valvesoftware.android.steam.community";

    private final Handler handler = new Handler(Looper.getMainLooper());
    private SteamSession session;
    private CompletableFuture<String> codeAnswer;
    private Bitmap heroImage;

    private ImageView hero;
    private View form;
    private View approve;
    private View code;
    private View qr;
    private TextView busy;
    private TextView privacy;
    private TextView formError;
    private TextView codeBody;
    private TextView codeError;
    private EditText accountField;
    private EditText passwordField;
    private EditText codeField;
    private ImageView qrImage;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        setContentView(R.layout.og_sign_in);
        View root = findViewById(R.id.og_root);
        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            Insets edges = insets.getInsets(WindowInsetsCompat.Type.displayCutout() | WindowInsetsCompat.Type.ime());
            v.setPadding(edges.left, edges.top, edges.right, edges.bottom);
            return insets;
        });
        hero = findViewById(R.id.og_hero);
        hero.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> alignHero());
        Art.load(this, Library.GAMES[1].appId, Art.HERO, bitmap -> {
            heroImage = bitmap;
            hero.setImageBitmap(bitmap);
            alignHero();
        });

        form = findViewById(R.id.og_form);
        approve = findViewById(R.id.og_approve);
        code = findViewById(R.id.og_code);
        qr = findViewById(R.id.og_qr);
        busy = findViewById(R.id.og_busy);
        privacy = findViewById(R.id.og_privacy);
        formError = findViewById(R.id.og_form_error);
        codeBody = findViewById(R.id.og_code_body);
        codeError = findViewById(R.id.og_code_error);
        accountField = findViewById(R.id.og_account_field);
        passwordField = findViewById(R.id.og_password_field);
        codeField = findViewById(R.id.og_code_field);
        qrImage = findViewById(R.id.og_qr_image);

        findViewById(R.id.og_submit).setOnClickListener(v -> signIn());
        passwordField.setOnEditorActionListener((v, action, event) -> {
            if (action != EditorInfo.IME_ACTION_GO) return false;
            signIn();
            return true;
        });
        findViewById(R.id.og_show_password).setOnClickListener(v -> {
            boolean hidden = (passwordField.getInputType() & InputType.TYPE_TEXT_VARIATION_PASSWORD) != 0;
            passwordField.setInputType(InputType.TYPE_CLASS_TEXT | (hidden ? InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD : InputType.TYPE_TEXT_VARIATION_PASSWORD));
            passwordField.setSelection(passwordField.length());
        });
        findViewById(R.id.og_use_qr).setOnClickListener(v -> signInWithQr());
        findViewById(R.id.og_not_now).setOnClickListener(v -> finish());
        findViewById(R.id.og_back).setOnClickListener(v -> back());
        findViewById(R.id.og_open_steam).setOnClickListener(v -> openSteamApp());
        findViewById(R.id.og_enter_code).setOnClickListener(v -> askCode(false, null, false));
        findViewById(R.id.og_code_submit).setOnClickListener(v -> submitCode());
        codeField.setOnEditorActionListener((v, action, event) -> {
            if (action != EditorInfo.IME_ACTION_GO) return false;
            submitCode();
            return true;
        });
        findViewById(R.id.og_qr_password).setOnClickListener(v -> {
            stopSession();
            show(form);
        });
        getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                back();
            }
        });
        show(form);
    }

    @Override
    protected void onResume() {
        super.onResume();
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowInsetsControllerCompat bars = WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        bars.setSystemBarsBehavior(WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        bars.hide(WindowInsetsCompat.Type.systemBars());
    }

    @Override
    protected void onDestroy() {
        stopSession();
        super.onDestroy();
    }

    private void alignHero() {
        if (heroImage == null || hero.getWidth() == 0 || hero.getHeight() == 0) return;
        float scale = Math.max((float) hero.getWidth() / heroImage.getWidth(), (float) hero.getHeight() / heroImage.getHeight());
        Matrix m = new Matrix();
        m.setScale(scale, scale);
        m.postTranslate(hero.getWidth() - heroImage.getWidth() * scale, (hero.getHeight() - heroImage.getHeight() * scale) / 2);
        hero.setImageMatrix(m);
    }

    private void show(View panel) {
        for (View v : new View[] {form, approve, code, qr, busy}) v.setVisibility(v == panel ? View.VISIBLE : View.GONE);
        privacy.setVisibility(panel == form ? View.VISIBLE : View.GONE);
    }

    private void showBusy(int text) {
        busy.setText(text);
        show(busy);
    }

    /** From a later step back to the form, from the form back to the library. */
    private void back() {
        if (form.getVisibility() == View.VISIBLE) {
            finish();
            return;
        }
        stopSession();
        show(form);
    }

    private void stopSession() {
        final SteamSession s = session;
        session = null;
        codeAnswer = null;
        // Signing off talks to Steam, so not on the main thread
        if (s != null) new Thread(s::close, "steam-close").start();
    }

    private static String deviceName() {
        return "OpenGothic on " + Build.MODEL;
    }

    private void signIn() {
        final String account = accountField.getText().toString().trim();
        final String password = passwordField.getText().toString();
        if (account.isEmpty() || password.isEmpty()) {
            fail(getString(R.string.og_enter_credentials));
            accountField.requestFocus();
            return;
        }
        hideKeyboard();
        showBusy(R.string.og_signing_in);
        final SteamSession s = new SteamSession();
        session = s;
        new Thread(() -> {
            try {
                s.connect();
                signedIn(s, s.login(account, password, deviceName(), prompt(s)));
            } catch (SteamSession.Failure e) {
                failed(s, e);
            }
        }, "steam-sign-in").start();
    }

    private void signInWithQr() {
        hideKeyboard();
        showBusy(R.string.og_connecting);
        final SteamSession s = new SteamSession();
        session = s;
        new Thread(() -> {
            try {
                s.connect();
                signedIn(s, s.loginQr(deviceName(), prompt(s)));
            } catch (SteamSession.Failure e) {
                failed(s, e);
            }
        }, "steam-sign-in").start();
    }

    private SteamSession.Prompt prompt(final SteamSession s) {
        return new SteamSession.Prompt() {
            @Override
            public void code(boolean email, String emailHint, boolean wrong, CompletableFuture<String> answer) {
                handler.post(() -> {
                    if (session != s) {
                        answer.complete(null);
                        return;
                    }
                    codeAnswer = answer;
                    askCode(email, emailHint, wrong);
                });
            }

            @Override
            public void approve() {
                handler.post(() -> {
                    if (session == s) show(approve);
                });
            }

            @Override
            public void qr(String url) {
                handler.post(() -> {
                    if (session != s) return;
                    qrImage.setImageBitmap(qrCode(url));
                    if (qr.getVisibility() != View.VISIBLE) show(qr);
                });
            }
        };
    }

    private void askCode(boolean email, String emailHint, boolean wrong) {
        codeBody.setText(!email ? getString(R.string.og_code_app)
                : emailHint != null ? getString(R.string.og_code_email, emailHint) : getString(R.string.og_code_email_unknown));
        codeError.setText(R.string.og_code_wrong);
        codeError.setVisibility(wrong ? View.VISIBLE : View.GONE);
        codeField.setText("");
        show(code);
        codeField.requestFocus();
    }

    private void submitCode() {
        final String value = codeField.getText().toString().trim().toUpperCase(Locale.ROOT);
        if (value.isEmpty()) return;
        hideKeyboard();
        CompletableFuture<String> answer = codeAnswer;
        codeAnswer = null;
        showBusy(R.string.og_signing_in);
        if (answer != null) {
            answer.complete(value);
            return;
        }
        // A code instead of approving in the app: the waiting sign-in finishes by itself once Steam accepts it
        final SteamSession s = session;
        if (s == null) return;
        new Thread(() -> {
            try {
                s.sendCode(value);
            } catch (SteamSession.Failure e) {
                handler.post(() -> {
                    if (session != s) return;
                    show(code);
                    codeError.setText(e.getMessage());
                    codeError.setVisibility(View.VISIBLE);
                });
            }
        }, "steam-code").start();
    }

    /** Keeps the sign-in and reads the library on the same connection before returning to it. */
    private void signedIn(SteamSession s, SteamStore.Session result) {
        handler.post(() -> showBusy(R.string.og_reading_library));
        SteamStore store = new SteamStore(getApplicationContext());
        final boolean kept = store.save(result);
        try {
            if (kept) s.readLibrary(store);
        } catch (SteamSession.Failure ignored) {
            // The library reads it again on its next refresh
        }
        s.close();
        handler.post(() -> {
            if (session != s) return;
            session = null;
            if (!kept) Toast.makeText(this, R.string.og_store_failed, Toast.LENGTH_LONG).show();
            setResult(RESULT_OK);
            finish();
        });
    }

    private void failed(SteamSession s, SteamSession.Failure e) {
        handler.post(() -> {
            if (session != s) return;
            stopSession();
            show(form);
            fail(e.getMessage());
        });
    }

    private void fail(String text) {
        formError.setText(text);
        formError.setVisibility(View.VISIBLE);
    }

    private void openSteamApp() {
        Intent intent = getPackageManager().getLaunchIntentForPackage(STEAM_APP);
        if (intent == null) Toast.makeText(this, R.string.og_no_steam_app, Toast.LENGTH_LONG).show();
        else startActivity(intent);
    }

    private void hideKeyboard() {
        InputMethodManager ime = getSystemService(InputMethodManager.class);
        View focus = getCurrentFocus();
        if (ime != null && focus != null) ime.hideSoftInputFromWindow(focus.getWindowToken(), 0);
    }

    /** Dark modules on the light frame, scaled in whole modules so the code stays sharp. */
    private Bitmap qrCode(String url) {
        try {
            BitMatrix m = new QRCodeWriter().encode(url, BarcodeFormat.QR_CODE, 400, 400, Collections.singletonMap(EncodeHintType.MARGIN, 0));
            int dark = ContextCompat.getColor(this, R.color.og_ground);
            int light = ContextCompat.getColor(this, R.color.og_text);
            int[] pixels = new int[m.getWidth() * m.getHeight()];
            for (int y = 0; y < m.getHeight(); ++y) {
                for (int x = 0; x < m.getWidth(); ++x) pixels[y * m.getWidth() + x] = m.get(x, y) ? dark : light;
            }
            return Bitmap.createBitmap(pixels, m.getWidth(), m.getHeight(), Bitmap.Config.ARGB_8888);
        } catch (WriterException e) {
            return null;
        }
    }
}
