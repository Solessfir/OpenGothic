package org.opengothic.app;

import android.app.Application;
import android.content.Context;

/** Gives background code, such as the Steam session and downloads, the texts in the phone's language. */
public final class App extends Application {
    private static Context context;

    @Override
    public void onCreate() {
        // JavaSteam keeps an 8 MB LZMA window per thread that ever unpacks a chunk, and Kotlin's shared IO pool grows to 64 threads.
        // Downloads filled the 512 MB heap that way; a small pool bounds the windows. Must be set before coroutines start.
        System.setProperty("kotlinx.coroutines.scheduler.max.pool.size", "12");
        super.onCreate();
        context = this;
    }

    static String text(int id, Object... args) {
        return context.getString(id, args);
    }
}
