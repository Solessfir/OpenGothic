package org.opengothic.app;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Handler;
import android.os.Looper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Library art from Steam's image servers: the same banner, logo and cover the Steam library shows.
 * Nothing ships in the APK; images are kept after the first download so the library also works offline.
 */
final class Art {
    static final String HERO = "library_hero.jpg";
    static final String LOGO = "logo.png";
    static final String COVER = "library_600x900.jpg";

    interface Callback {
        void loaded(Bitmap image);
    }

    private static final String BASE = "https://shared.steamstatic.com/store_item_assets/steam/apps/";
    private static final ExecutorService pool = Executors.newFixedThreadPool(2);
    private static final Handler main = new Handler(Looper.getMainLooper());
    private static final Map<String, Bitmap> memory = new HashMap<>();

    private Art() {
    }

    /** Calls back on the main thread, only when the image exists. */
    static void load(Context context, int appId, String name, Callback callback) {
        load(context, appId + "_" + name, BASE + appId + "/" + name, callback);
    }

    /** Any image by URL; the key names the cached file, so a new URL needs a new key. */
    static void load(Context context, final String key, final String url, Callback callback) {
        synchronized (memory) {
            Bitmap cached = memory.get(key);
            if (cached != null) {
                callback.loaded(cached);
                return;
            }
        }
        final File file = new File(new File(context.getFilesDir(), "art"), key);
        pool.execute(() -> {
            Bitmap image = file.isFile() ? BitmapFactory.decodeFile(file.getPath()) : null;
            if (image == null && fetch(url, file)) image = BitmapFactory.decodeFile(file.getPath());
            if (image == null) return;
            final Bitmap result = image;
            synchronized (memory) {
                memory.put(key, result);
            }
            main.post(() -> callback.loaded(result));
        });
    }

    private static boolean fetch(String url, File file) {
        File parent = file.getParentFile();
        if (parent == null || (!parent.isDirectory() && !parent.mkdirs())) return false;
        File part = new File(parent, file.getName() + ".part");
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(url).openConnection();
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) return false;
            try (InputStream in = connection.getInputStream(); FileOutputStream out = new FileOutputStream(part)) {
                byte[] buffer = new byte[64 * 1024];
                int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
            }
            return part.renameTo(file);
        } catch (IOException e) {
            return false;
        } finally {
            if (connection != null) connection.disconnect();
            if (part.exists() && !part.delete()) part.deleteOnExit();
        }
    }
}
