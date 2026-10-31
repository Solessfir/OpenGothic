package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Build;

import org.json.JSONObject;
import org.opengothic.launcher.R;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Problem reports as issues in the developer's private GitHub repository: the game's logs, how Android ended the game process,
 * and the phone. A save goes along only when the player picks one; Steam data stays out.
 */
final class Reports {
    /** GitHub takes 65536 characters per issue. */
    private static final int ISSUE_SIZE = 60000;

    private Reports() {
    }

    /** The report was filed, but its save did not arrive. */
    static final class SaveNotSent extends IOException {
        SaveNotSent(String message) {
            super(message);
        }
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

    /** The game's saves, newest first. */
    static List<File> saves(Context c, Library.Game game) {
        File root = Library.root(c);
        File[] files = root == null || game == null ? null : Library.user(root, game).listFiles((dir, name) -> name.endsWith(".sav"));
        List<File> ret = new ArrayList<>();
        if (files != null) Collections.addAll(ret, files);
        ret.sort((a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        return ret;
    }

    /** The name the player gave the save, from the header OpenGothic writes first into the archive. */
    static String saveTitle(File save) {
        try (ZipFile zip = new ZipFile(save)) {
            ZipEntry header = zip.getEntry("header");
            if (header == null) return save.getName();
            byte[] data = new byte[(int) Math.min(header.getSize(), 4096)];
            try (InputStream in = zip.getInputStream(header)) {
                int got = 0;
                for (int n; got < data.length && (n = in.read(data, got, data.length - got)) > 0; ) got += n;
            }
            // The tag "OpenGothic/Save" and its terminating zero, a 16-bit version, then the name as a 32-bit length and its bytes
            ByteBuffer b = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
            b.position(16 + 2);
            int length = b.getInt();
            if (length <= 0 || length > b.remaining()) return save.getName();
            String title = new String(data, b.position(), length, StandardCharsets.UTF_8).trim();
            return title.isEmpty() ? save.getName() : title;
        } catch (IOException | RuntimeException e) {
            return save.getName();
        }
    }

    /** The screenshot OpenGothic stores in the save, scaled down to about the given width. */
    static Bitmap savePreview(File save, int width) {
        try (ZipFile zip = new ZipFile(save)) {
            ZipEntry entry = zip.getEntry("preview.jpg");
            if (entry == null) return null;
            BitmapFactory.Options size = new BitmapFactory.Options();
            size.inJustDecodeBounds = true;
            try (InputStream in = zip.getInputStream(entry)) {
                BitmapFactory.decodeStream(in, null, size);
            }
            BitmapFactory.Options scaled = new BitmapFactory.Options();
            scaled.inSampleSize = 1;
            while (size.outWidth / (scaled.inSampleSize * 2) >= width) scaled.inSampleSize *= 2;
            try (InputStream in = zip.getInputStream(entry)) {
                return BitmapFactory.decodeStream(in, null, scaled);
            }
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    /**
     * Sends the report to the relay named in the launcher config, which files the issue. The relay keeps the GitHub token,
     * so the app never holds one. A save follows the report, which the relay links from the issue.
     */
    static void send(Context c, Library.Game game, String problem, String note, boolean suggestion, File save) throws IOException {
        String relay = Updates.reportRelay(c);
        if (relay == null) throw new IOException(App.text(R.string.og_err_reports_unavailable));
        Map<String, String> report = new HashMap<>();
        if (suggestion) {
            String line = note.split("\n", 2)[0];
            report.put("title", "Suggestion: " + (line.length() > 80 ? line.substring(0, 80) + "…" : line));
            report.put("body", note + "\n\n" + summary(c, game));
        } else {
            report.put("title", (problem != null ? problem : game.name + " problem") + " on " + Build.MANUFACTURER + " " + Build.MODEL
                    + " (OpenGothic " + Updates.installedVersion(c) + ")");
            report.put("body", body(c, game, note));
        }
        HttpURLConnection connection = (HttpURLConnection) new URL(relay).openConnection();
        try {
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setDoOutput(true);
            connection.setRequestProperty("Content-Type", "application/json");
            try (OutputStream out = connection.getOutputStream()) {
                out.write(new JSONObject(report).toString().getBytes(StandardCharsets.UTF_8));
            }
            int code = connection.getResponseCode();
            if (code == 429) throw new IOException(App.text(R.string.og_err_reports_limit));
            if (code / 100 != 2) throw new IOException("The report server answered " + code + ".");
            if (save != null) {
                try {
                    sendSave(relay, new JSONObject(read(connection.getInputStream())), save);
                } catch (IOException | org.json.JSONException e) {
                    throw new SaveNotSent(e.getMessage());
                }
            }
        } finally {
            connection.disconnect();
        }
    }

    private static void sendSave(String relay, JSONObject filed, File save) throws IOException, org.json.JSONException {
        String query = "?issue=" + filed.getInt("number") + "&key=" + filed.getString("upload") + "&name=" + save.getName();
        HttpURLConnection connection = (HttpURLConnection) new URL(relay.replaceAll("/+$", "") + "/save" + query).openConnection();
        try {
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(120000);
            connection.setDoOutput(true);
            connection.setRequestMethod("PUT");
            connection.setRequestProperty("Content-Type", "application/octet-stream");
            connection.setFixedLengthStreamingMode(save.length());
            try (InputStream in = new FileInputStream(save); OutputStream out = connection.getOutputStream()) {
                byte[] buffer = new byte[64 * 1024];
                for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
            }
            int code = connection.getResponseCode();
            if (code / 100 != 2) throw new IOException("The report server answered " + code + ".");
        } finally {
            connection.disconnect();
        }
    }

    private static String read(InputStream in) throws IOException {
        StringBuilder b = new StringBuilder();
        try (BufferedReader r = new BufferedReader(new InputStreamReader(in, StandardCharsets.UTF_8))) {
            for (String line; (line = r.readLine()) != null; ) b.append(line);
        }
        return b.toString();
    }

    private static String body(Context c, Library.Game game, String note) throws IOException {
        StringBuilder b = new StringBuilder();
        if (!note.isEmpty()) b.append(note).append("\n\n");
        b.append(summary(c, game)).append("\n\n");
        StringBuilder exits = new StringBuilder();
        for (ApplicationExitInfo e : gameExits(c)) exits.append(e).append('\n');
        section(b, "Game exits, newest first", exits.toString());
        File root = Library.root(c);
        if (root != null) {
            File user = Library.user(root, game);
            section(b, written("crash.log", new File(user, "crash.log")), tail(new File(user, "crash.log"), 8000));
            section(b, "log.txt", tail(new File(user, "log.txt"), 8000));
            section(b, "steam-error.txt", tail(new File(root, "steam-error.txt"), 4000));
        }
        // Apps read only their own log, which holds the engine's output and crashes of the game process too.
        // The running launcher's lines are left out: its dialogs and keyboard would push the crash out of the issue.
        String self = Integer.toString(android.os.Process.myPid());
        StringBuilder log = new StringBuilder();
        Process logcat = new ProcessBuilder("logcat", "-d", "-v", "threadtime").redirectErrorStream(true).start();
        try (BufferedReader in = new BufferedReader(new InputStreamReader(logcat.getInputStream(), StandardCharsets.UTF_8))) {
            String line;
            while ((line = in.readLine()) != null) {
                String[] field = line.trim().split("\\s+", 4);
                if (field.length < 3 || !field[2].equals(self)) log.append(line).append('\n');
            }
        } finally {
            logcat.destroy();
        }
        int room = ISSUE_SIZE - b.length() - 100;
        if (room > 0) section(b, "logcat", log.substring(Math.max(0, log.length() - room)));
        return b.toString();
    }

    /** One line about the launcher and the phone. */
    private static String summary(Context c, Library.Game game) {
        ActivityManager.MemoryInfo memory = new ActivityManager.MemoryInfo();
        c.getSystemService(ActivityManager.class).getMemoryInfo(memory);
        String soc = Build.VERSION.SDK_INT >= 31 ? ", " + Build.SOC_MANUFACTURER + " " + Build.SOC_MODEL : "";
        return String.format(Locale.ROOT, "OpenGothic %s, %s on %s %s%s, Android %s, %.1f GB RAM",
                Updates.installedVersion(c), game.title, Build.MANUFACTURER, Build.MODEL, soc, Build.VERSION.RELEASE, memory.totalMem / 1e9);
    }

    /** The date tells whether a crash log belongs to this problem or an older one. */
    private static String written(String name, File f) {
        return name + ", written " + new SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.ROOT).format(new Date(f.lastModified()));
    }

    private static void section(StringBuilder b, String title, String text) {
        if (text == null || text.isEmpty()) return;
        b.append("### ").append(title).append("\n````\n").append(text).append("\n````\n\n");
    }

    /** The end of a file, where the newest lines are. */
    private static String tail(File f, int max) throws IOException {
        if (!f.isFile()) return null;
        try (RandomAccessFile in = new RandomAccessFile(f, "r")) {
            long start = Math.max(0, in.length() - max);
            byte[] data = new byte[(int) (in.length() - start)];
            in.seek(start);
            in.readFully(data);
            return new String(data, StandardCharsets.UTF_8);
        }
    }
}
