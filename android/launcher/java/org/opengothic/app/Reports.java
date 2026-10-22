package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;

import androidx.core.content.FileProvider;

import org.opengothic.launcher.R;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** A ZIP that shows why a game failed: its logs, how Android ended the game process, and the phone. Saves and Steam data stay out. */
final class Reports {
    private Reports() {
    }

    /** The game started last, see LauncherActivity.play. */
    static Library.Game lastPlayed(File root) {
        try (BufferedReader in = new BufferedReader(new FileReader(new File(root, "game.txt")))) {
            String line = in.readLine();
            return line == null ? null : Library.byFolder(line.trim());
        } catch (IOException e) {
            return null;
        }
    }

    /** How the game process ended, newest first. Empty before Android 11. */
    static List<ApplicationExitInfo> gameExits(Context c) {
        if (Build.VERSION.SDK_INT < 30) return Collections.emptyList();
        List<ApplicationExitInfo> ret = new ArrayList<>();
        for (ApplicationExitInfo e : c.getSystemService(ActivityManager.class).getHistoricalProcessExitReasons(c.getPackageName(), 0, 0)) {
            if (e.getProcessName().endsWith(":game")) ret.add(e);
        }
        return ret;
    }

    /** What went wrong, or 0 when the player quit or left the game and Android closed it later. */
    @TargetApi(30)
    static int problem(ApplicationExitInfo e) {
        boolean shown = e.getImportance() <= ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
        switch (e.getReason()) {
            case ApplicationExitInfo.REASON_CRASH:
            case ApplicationExitInfo.REASON_CRASH_NATIVE:
                return R.string.og_problem_crash;
            case ApplicationExitInfo.REASON_ANR:
                return R.string.og_problem_frozen;
            case ApplicationExitInfo.REASON_EXIT_SELF:
                return e.getStatus() != 0 ? R.string.og_problem_error : 0;
            case ApplicationExitInfo.REASON_LOW_MEMORY:
                return shown ? R.string.og_problem_memory : 0;
            case ApplicationExitInfo.REASON_SIGNALED:
            case ApplicationExitInfo.REASON_INITIALIZATION_FAILURE:
                return shown ? R.string.og_problem_closed : 0;
            default:
                return 0;
        }
    }

    /** Writes the report into the app's cache, where only the share sheet reads it. */
    static File write(Context c, Library.Game game) throws IOException {
        File dir = new File(c.getCacheDir(), "reports");
        GameFiles.delete(dir);
        if (!dir.mkdirs()) throw new IOException("Cannot write the report.");
        String date = new SimpleDateFormat("yyyy-MM-dd", Locale.ROOT).format(new Date());
        File file = new File(dir, "OpenGothic " + game.name + " report " + date + ".zip");
        File root = Library.root(c);
        try (ZipOutputStream zip = new ZipOutputStream(new FileOutputStream(file))) {
            List<ApplicationExitInfo> exits = gameExits(c);
            StringBuilder text = new StringBuilder(summary(c, game));
            text.append("\nGame exits, newest first:\n");
            for (ApplicationExitInfo e : exits) text.append(e).append('\n');
            add(zip, new ZipEntry("report.txt"), new ByteArrayInputStream(text.toString().getBytes(StandardCharsets.UTF_8)));
            if (root != null) {
                File user = Library.user(root, game);
                add(zip, new File(user, "log.txt"));
                add(zip, new File(user, "crash.log"));
                add(zip, new File(root, "steam-error.txt"));
            }
            traces(zip, exits);
            // Apps read only their own log, which holds the engine's output of the game process too
            Process logcat = new ProcessBuilder("logcat", "-d", "-v", "threadtime").redirectErrorStream(true).start();
            try (InputStream in = logcat.getInputStream()) {
                add(zip, new ZipEntry("logcat.txt"), in);
            } finally {
                logcat.destroy();
            }
        }
        return file;
    }

    /** A native crash comes with Android's tombstone (protobuf), a freeze with the threads' stacks. */
    @TargetApi(30)
    private static void traces(ZipOutputStream zip, List<ApplicationExitInfo> exits) throws IOException {
        for (ApplicationExitInfo e : exits) {
            try (InputStream trace = e.getTraceInputStream()) {
                if (trace != null) add(zip, new ZipEntry("trace-" + e.getTimestamp() + "-" + e.getReason()), trace);
            }
        }
    }

    /** One line for the message the report is shared with. */
    static String summary(Context c, Library.Game game) {
        ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
        c.getSystemService(ActivityManager.class).getMemoryInfo(memory);
        String soc = Build.VERSION.SDK_INT >= 31 ? ", " + Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL : "";
        return String.format(Locale.ROOT, "OpenGothic %s, %s on %s %s%s, Android %s, %.1f GB RAM",
                Updates.installedVersion(c), game.title, Build.MANUFACTURER, Build.MODEL, soc, Build.VERSION.RELEASE, memory.totalMem / 1e9);
    }

    static Intent share(Context c, File report, String text) {
        Uri uri = FileProvider.getUriForFile(c, c.getPackageName() + ".files", report);
        Intent send = new Intent(Intent.ACTION_SEND).setType("application/zip")
                .putExtra(Intent.EXTRA_STREAM, uri)
                .putExtra(Intent.EXTRA_SUBJECT, report.getName())
                .putExtra(Intent.EXTRA_TEXT, text)
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        // The clip data carries the read grant to the share sheet too, so it can show the file
        send.setClipData(ClipData.newRawUri(null, uri));
        return Intent.createChooser(send, c.getString(R.string.og_report_send));
    }

    private static void add(ZipOutputStream zip, File f) throws IOException {
        if (!f.isFile()) return;
        ZipEntry entry = new ZipEntry(f.getName());
        // The date tells whether a crash log belongs to this problem or an older one
        entry.setTime(f.lastModified());
        try (InputStream in = new FileInputStream(f)) {
            add(zip, entry, in);
        }
    }

    private static void add(ZipOutputStream zip, ZipEntry entry, InputStream in) throws IOException {
        zip.putNextEntry(entry);
        byte[] buffer = new byte[64 * 1024];
        int count;
        while ((count = in.read(buffer)) != -1) zip.write(buffer, 0, count);
        zip.closeEntry();
    }
}
