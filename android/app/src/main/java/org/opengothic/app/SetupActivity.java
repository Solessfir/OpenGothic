package org.opengothic.app;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/** Owns game-data setup; engine lifecycle and input remain in Tempest. */
public final class SetupActivity extends Activity {
    private static final int PICK_ARCHIVE = 1;
    private static volatile boolean running;
    private static volatile boolean ready;
    private static volatile String message = "";
    private static volatile int percent;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private TextView status;
    private ProgressBar progress;
    private Button choose;
    private Button retry;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (24 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);
        status = new TextView(this);
        status.setTextSize(18);
        layout.addView(status);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        layout.addView(progress);
        choose = new Button(this);
        choose.setText("Choose private-game.zip");
        choose.setOnClickListener(v -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, PICK_ARCHIVE);
        });
        layout.addView(choose);
        retry = new Button(this);
        retry.setText("Retry bundled files / check copied files");
        retry.setOnClickListener(v -> begin(null));
        layout.addView(retry);
        setContentView(layout);
        if (!running) begin(null);
    }

    private void begin(Uri uri) {
        if (running) return;
        ready = false;
        percent = 0;
        File root = getExternalFilesDir(null);
        File pending = new File(getFilesDir(), "private-assets-pending");
        if (root == null) {
            message = "App storage is unavailable. Unlock the phone and retry.";
            return;
        }
        InputStream source;
        try {
            source = uri == null ? getAssets().open("private-game.zip") : getContentResolver().openInputStream(uri);
            if (source == null) throw new IOException("Cannot open selected archive");
        } catch (IOException e) {
            File data = new File(root, "Gothic2/Data");
            if (uri == null && !pending.exists() && data.isDirectory() && data.list() != null && data.list().length > 0) {
                launchGame();
                return;
            }
            message = "Copy private-game.zip to Downloads and select it here. No storage permission is needed.\n\n" +
                    "Alternatively copy your game installation into:\n" + new File(root, "Gothic2") + "\n\n" + e.getMessage();
            return;
        }
        running = true;
        message = "Checking game files. First launch can take several minutes; keep this screen open.";
        File marker = new File(getFilesDir(), "private-assets-index.tsv");
        // The worker holds no Activity reference. A recreated Activity observes its state.
        new Thread(() -> {
            try (InputStream input = source) {
                if (!pending.exists() && !pending.createNewFile()) throw new IOException("Cannot record pending setup");
                PrivateAssets.extract(input, root, marker, (done, total, path) -> {
                    percent = (int) (done * 100 / Math.max(1, total));
                    message = "Preparing game files: " + percent + "%\n" + path;
                });
                if (!pending.delete()) throw new IOException("Cannot finish pending setup");
                ready = true;
            } catch (IOException | RuntimeException e) {
                message = "Setup stopped: " + e.getMessage() + "\n\nFree space or correct the problem, then retry. Completed files, saves and settings are preserved.";
            } finally {
                running = false;
            }
        }, "OpenGothic asset setup").start();
    }

    private final Runnable refresh = new Runnable() {
        @Override public void run() {
            status.setText(message);
            progress.setProgress(percent);
            choose.setEnabled(!running);
            retry.setEnabled(!running);
            if (ready && !running) {
                ready = false;
                launchGame();
            } else {
                handler.postDelayed(this, 250);
            }
        }
    };

    private void launchGame() {
        startActivity(new Intent(this, org.tempest.TempestNativeActivity.class));
        finish();
    }

    @Override protected void onResume() {
        super.onResume();
        handler.post(refresh);
    }

    @Override protected void onPause() {
        handler.removeCallbacks(refresh);
        super.onPause();
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == PICK_ARCHIVE && result == RESULT_OK && data != null && data.getData() != null) begin(data.getData());
    }
}
