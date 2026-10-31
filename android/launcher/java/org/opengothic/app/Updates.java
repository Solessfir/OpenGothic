package org.opengothic.app;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;

import androidx.core.content.FileProvider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.opengothic.launcher.R;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

/**
 * Updates from the fork's GitHub releases: a release tagged launcher-<version> with the APK attached.
 * The APK has to be signed with the same key as the installed one, or Android refuses the update.
 * The same check reads the launcher config from the fork.
 */
final class Updates {
    private static final String RELEASES = "https://api.github.com/repos/Solessfir/OpenGothic/releases?per_page=20";
    private static final String TAG = "launcher-";
    private static final String ASSET = "GothicLauncher-arm64.apk";
    private static final long CHECK_INTERVAL = 6L * 60 * 60 * 1000;
    /** Settings that change without a new release: phones known not to work and where problem reports go. */
    private static final String CONFIG = "https://raw.githubusercontent.com/Solessfir/OpenGothic/android/android/launcher-config.json";

    static final class Release {
        final String version;
        final String notes;
        final String url;
        final long size;

        Release(String version, String notes, String url, long size) {
            this.version = version;
            this.notes = notes;
            this.url = url;
            this.size = size;
        }
    }

    static volatile boolean downloading;
    /** 0..100 while downloading. */
    static volatile int percent;
    static volatile String error;
    /** Set when the download is complete and the installer should open. */
    static volatile File ready;

    private Updates() {
    }

    private static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("updates", Context.MODE_PRIVATE);
    }

    static String installedVersion(Context c) {
        try {
            String name = c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
            return name == null ? "0" : name;
        } catch (PackageManager.NameNotFoundException e) {
            return "0";
        }
    }

    /**
     * True once on the first start after an update. Launchers before 0.6.11 did not record their version, but their
     * last check stored the release it offered; a fresh install has neither and shows nothing.
     */
    static boolean justUpdated(Context c) {
        SharedPreferences p = prefs(c);
        String installed = installedVersion(c);
        String lastRun = p.getString("lastRun", null);
        if (installed.equals(lastRun)) return false;
        p.edit().putString("lastRun", installed).apply();
        return lastRun != null || installed.equals(p.getString("version", null));
    }

    /** The newest release found earlier, if it is newer than this app. */
    static Release available(Context c) {
        SharedPreferences p = prefs(c);
        String version = p.getString("version", null);
        if (version == null || compare(version, installedVersion(c)) <= 0) return null;
        return new Release(version, p.getString("notes", ""), p.getString("url", ""), p.getLong("size", 0));
    }

    private static long checked;

    /** Asks GitHub when the launcher starts and every few hours while it stays open; the answer shows on the next render. */
    static void check(Context context) {
        final Context app = context.getApplicationContext();
        final SharedPreferences p = prefs(app);
        long now = System.currentTimeMillis();
        if (checked != 0 && now - checked < CHECK_INTERVAL) return;
        checked = now;
        new Thread(() -> {
            try {
                Release newest = newest(new JSONArray(fetch(RELEASES)));
                SharedPreferences.Editor e = p.edit().remove("version");
                if (newest != null) {
                    e.putString("version", newest.version).putString("notes", newest.notes)
                            .putString("url", newest.url).putLong("size", newest.size);
                }
                e.apply();
            } catch (IOException | JSONException ignored) {
                // Offline or rate limited: the release found before stays shown
            }
            try {
                String config = fetch(CONFIG);
                new JSONObject(config);
                p.edit().putString("config", config).apply();
            } catch (IOException | JSONException ignored) {
                // The config fetched before stays in use
            }
        }, "update-check").start();
    }

    private static JSONObject config(Context c) {
        try {
            return new JSONObject(prefs(c).getString("config", "{}"));
        } catch (JSONException e) {
            return new JSONObject();
        }
    }

    /** Where problem reports go, null until the config names it. */
    static String reportRelay(Context c) {
        String url = config(c).optString("reports");
        return url.startsWith("https://") ? url : null;
    }

    /**
     * Why this phone is known not to run the games, or null. Each entry of the config's list is a regular expression
     * searched in the manufacturer, model, hardware and chip names, with the reason the player sees.
     */
    static String unsupported(Context c) {
        if (!c.getPackageManager().hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, 0x00401000)) {
            return c.getString(R.string.og_no_vulkan);
        }
        String device = Build.MANUFACTURER + " " + Build.MODEL + " " + Build.HARDWARE + (Build.VERSION.SDK_INT >= 31 ? " " + Build.SOC_MODEL : "");
        JSONArray list = config(c).optJSONArray("unsupported");
        for (int i = 0; list != null && i < list.length(); ++i) {
            JSONObject entry = list.optJSONObject(i);
            String match = entry == null ? "" : entry.optString("match");
            if (match.isEmpty()) continue;
            try {
                if (Pattern.compile(match, Pattern.CASE_INSENSITIVE).matcher(device).find()) return entry.optString("reason");
            } catch (PatternSyntaxException ignored) {
                // A broken entry warns nobody
            }
        }
        return null;
    }

    private static Release newest(JSONArray releases) throws JSONException {
        Release best = null;
        for (int i = 0; i < releases.length(); ++i) {
            JSONObject r = releases.getJSONObject(i);
            String tag = r.optString("tag_name");
            if (r.optBoolean("draft") || !tag.startsWith(TAG)) continue;
            String version = tag.substring(TAG.length());
            JSONArray assets = r.optJSONArray("assets");
            for (int a = 0; assets != null && a < assets.length(); ++a) {
                JSONObject asset = assets.getJSONObject(a);
                if (!ASSET.equals(asset.optString("name"))) continue;
                if (best == null || compare(version, best.version) > 0) {
                    best = new Release(version, r.optString("body", "").trim(), asset.optString("browser_download_url"), asset.optLong("size"));
                }
            }
        }
        return best;
    }

    /** Dotted numbers, "0.10" after "0.9"; anything after the numbers ("-android") is ignored. */
    static int compare(String a, String b) {
        String[] x = a.split("[^0-9]+", -1);
        String[] y = b.split("[^0-9]+", -1);
        for (int i = 0; i < Math.max(x.length, y.length); ++i) {
            long u = i < x.length && !x[i].isEmpty() ? Long.parseLong(x[i]) : 0;
            long v = i < y.length && !y[i].isEmpty() ? Long.parseLong(y[i]) : 0;
            if (u != v) return u < v ? -1 : 1;
        }
        return 0;
    }

    static void download(Context context, final Release release) {
        if (downloading) return;
        final Context app = context.getApplicationContext();
        downloading = true;
        percent = 0;
        error = null;
        ready = null;
        new Thread(() -> {
            File dir = new File(app.getCacheDir(), "updates");
            File apk = new File(dir, ASSET);
            File part = new File(dir, ASSET + ".part");
            HttpURLConnection connection = null;
            try {
                if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException(App.text(R.string.og_err_store_update));
                connection = (HttpURLConnection) new URL(release.url).openConnection();
                connection.setConnectTimeout(15000);
                connection.setReadTimeout(30000);
                if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) throw new IOException("GitHub answered " + connection.getResponseCode() + ".");
                long total = release.size > 0 ? release.size : connection.getContentLengthLong();
                try (InputStream in = connection.getInputStream(); FileOutputStream out = new FileOutputStream(part)) {
                    byte[] buffer = new byte[256 * 1024];
                    long done = 0;
                    int count;
                    while ((count = in.read(buffer)) != -1) {
                        out.write(buffer, 0, count);
                        done += count;
                        if (total > 0) percent = (int) Math.min(100, done * 100 / total);
                    }
                }
                if (apk.exists() && !apk.delete()) throw new IOException(App.text(R.string.og_err_store_update));
                if (!part.renameTo(apk)) throw new IOException(App.text(R.string.og_err_store_update));
                ready = apk;
            } catch (IOException e) {
                error = App.text(R.string.og_update_failed, e.getMessage());
                if (part.exists() && !part.delete()) part.deleteOnExit();
            } finally {
                if (connection != null) connection.disconnect();
                downloading = false;
            }
        }, "update-download").start();
    }

    /** Android's own install screen for the downloaded APK. */
    static Intent installIntent(Context c, File apk) {
        Uri uri = FileProvider.getUriForFile(c, c.getPackageName() + ".files", apk);
        return new Intent(Intent.ACTION_VIEW)
                .setDataAndType(uri, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
    }

    private static String fetch(String url) throws IOException {
        HttpURLConnection connection = (HttpURLConnection) new URL(url).openConnection();
        try {
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setRequestProperty("Accept", "application/vnd.github+json");
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) throw new IOException("GitHub answered " + connection.getResponseCode());
            try (InputStream in = connection.getInputStream()) {
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] buffer = new byte[16 * 1024];
                int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
                return new String(out.toByteArray(), StandardCharsets.UTF_8);
            }
        } finally {
            connection.disconnect();
        }
    }
}
