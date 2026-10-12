package org.opengothic.app;

import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;
import android.util.Log;
import org.opengothic.launcher.R;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.util.ArrayDeque;
import java.util.Collections;
import java.util.Date;

/**
 * Downloads games from Steam one after another, also while the launcher is closed. Each game goes to a staging folder
 * first; only the files the engine reads are moved into place, so a game never appears half installed.
 */
@TargetApi(26)
public final class DownloadService extends Service {
    private static final String ACTION_START = "org.opengothic.app.DOWNLOAD";
    private static final String ACTION_PAUSE = "org.opengothic.app.PAUSE";
    private static final String ACTION_CANCEL = "org.opengothic.app.CANCEL";
    private static final String EXTRA_APP = "app";
    private static final String EXTRA_LANGUAGE = "language";
    private static final String CHANNEL = "downloads";
    private static final int NOTIFICATION = 1;

    private final ArrayDeque<Downloads.State> queue = new ArrayDeque<>();
    private volatile Downloads.State current;
    private volatile SteamSession session;
    private Thread worker;
    private PowerManager.WakeLock wake;

    static void start(Context c, Library.Game game, String language) {
        c.startForegroundService(intent(c, ACTION_START, game).putExtra(EXTRA_LANGUAGE, language));
    }

    /** Stops the download and keeps the files, so it continues later. */
    static void pause(Context c, Library.Game game) {
        c.startService(intent(c, ACTION_PAUSE, game));
    }

    /** Stops the download and deletes what it downloaded. */
    static void cancel(Context c, Library.Game game) {
        c.startService(intent(c, ACTION_CANCEL, game));
    }

    private static Intent intent(Context c, String action, Library.Game game) {
        return new Intent(c, DownloadService.class).setAction(action).putExtra(EXTRA_APP, game.appId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Library.Game game = intent == null ? null : Library.byApp(intent.getIntExtra(EXTRA_APP, 0));
        if (game == null) {
            if (worker == null) stopSelf();
            return START_NOT_STICKY;
        }
        String action = intent.getAction();
        if (ACTION_START.equals(action)) {
            startForeground(notification(game.title, App.text(R.string.og_waiting), 0, game));
            Downloads.State s = Downloads.get(game);
            if (s == null || !s.active()) {
                String language = intent.getStringExtra(EXTRA_LANGUAGE);
                s = new Downloads.State(game, language == null ? "" : language);
                Downloads.put(s);
                synchronized (queue) {
                    queue.add(s);
                }
            }
            startWorker();
        } else {
            boolean delete = ACTION_CANCEL.equals(action);
            Downloads.State s = Downloads.get(game);
            boolean waiting;
            synchronized (queue) {
                waiting = s != null && queue.remove(s);
            }
            if (s != null && s == current) {
                s.stop = delete ? Downloads.State.CANCEL : Downloads.State.PAUSE;
                SteamSession steam = session;
                if (steam != null) steam.cancel();
            } else {
                if (waiting || s == null || !s.active()) Downloads.forget(game);
                if (delete) removeStaging(game);
                if (worker == null) stopSelf();
            }
        }
        return START_NOT_STICKY;
    }

    private void startWorker() {
        if (worker != null) return;
        guardLibraryThreads();
        PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        if (pm != null && wake == null) {
            wake = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "opengothic:download");
            wake.acquire(12L * 60 * 60 * 1000);
        }
        worker = new Thread(() -> {
            while (true) {
                Downloads.State s;
                synchronized (queue) {
                    s = queue.poll();
                    if (s == null) {
                        worker = null;
                        break;
                    }
                }
                current = s;
                run(s);
                current = null;
            }
            if (wake != null && wake.isHeld()) wake.release();
            wake = null;
            stopForeground(STOP_FOREGROUND_DETACH);
            stopSelf();
        }, "steam-download");
        worker.start();
    }

    private void run(final Downloads.State s) {
        final Library.Game game = s.game;
        SteamSession steam = null;
        File root = Library.root(this);
        File staging = root == null ? null : Library.staging(root, game);
        try {
            s.queued = false;
            s.status = App.text(R.string.og_connecting);
            update(s);
            SteamStore store = new SteamStore(this);
            SteamStore.Session saved = store.load();
            if (root == null) throw new SteamSession.Failure(App.text(R.string.og_storage_unavailable), false);
            if (saved == null) throw new SteamSession.Failure(App.text(R.string.og_err_sign_in_first), true);
            steam = new SteamSession();
            session = steam;
            steam.connect();
            steam.resume(saved);
            if (game.free && !steam.owned(Collections.singleton(game.appId)).contains(game.appId)) {
                s.status = App.text(R.string.og_adding_to_library);
                update(s);
                steam.claimFree(game.appId);
            }
            steam.download(game.appId, staging, s.language, new SteamSession.Progress() {
                @Override
                public void status(String text) {
                    s.status = text;
                }

                @Override
                public void bytes(long done, long total) {
                    s.progress(done, total);
                    update(s);
                }
            });

            s.status = App.text(R.string.og_finishing);
            s.total = 0;
            update(s);
            GameFiles.prune(staging);
            String found = GameFiles.detect(staging);
            if (!game.folder.equals(found)) throw new SteamSession.Failure(App.text(R.string.og_err_unrecognized, game.title), false);
            // The folder stays, so trying again validates it with Steam and downloads only the damaged parts
            if (!GameFiles.damaged(staging).isEmpty()) throw new SteamSession.Failure(App.text(R.string.og_err_damaged), false);
            File target = new File(root, game.folder);
            if (Library.installed(root, game)) throw new SteamSession.Failure(game.title + " is already installed.", false);
            if (target.exists()) GameFiles.delete(target);
            if (!staging.renameTo(target)) throw new SteamSession.Failure(App.text(R.string.og_err_move_game), false);
            s.done = true;
        } catch (SteamSession.Failure e) {
            if (e.signIn) new SteamStore(this).clear();
            // Keep a reason found earlier, such as the library running out of memory, over the stop it caused
            if (s.stop == Downloads.State.NONE && s.error == null) s.error = e.getMessage();
        } catch (Throwable e) {
            Log.e("OpenGothic/Steam", "download", e);
            record(e);
            if (s.stop == Downloads.State.NONE && s.error == null) s.error = e instanceof OutOfMemoryError ? App.text(R.string.og_err_memory) : App.text(R.string.og_err_download, e);
        } finally {
            session = null;
            if (steam != null) steam.close();
        }
        if (s.stop != Downloads.State.NONE) {
            Downloads.forget(game);
            if (s.stop == Downloads.State.CANCEL) removeStaging(game);
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.cancel(NOTIFICATION);
        } else {
            finished(s);
        }
    }

    private void removeStaging(Library.Game game) {
        File root = Library.root(this);
        if (root == null) return;
        try {
            GameFiles.delete(Library.staging(root, game));
        } catch (IOException e) {
            Log.w("OpenGothic/Steam", "cannot remove a cancelled download", e);
        }
    }

    /** JavaSteam runs its own threads; an error there (usually memory) ends the download instead of the app. */
    private void guardLibraryThreads() {
        final Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        if (previous instanceof Guard) return;
        Thread.setDefaultUncaughtExceptionHandler(new Guard() {
            @Override
            public void uncaughtException(Thread thread, Throwable error) {
                Downloads.State s = current;
                SteamSession steam = session;
                if (s == null || steam == null) {
                    if (previous != null) previous.uncaughtException(thread, error);
                    return;
                }
                Log.e("OpenGothic/Steam", "uncaught in " + thread.getName(), error);
                record(error);
                s.error = error instanceof OutOfMemoryError ? App.text(R.string.og_err_memory) : App.text(R.string.og_err_download, error);
                steam.cancel();
            }
        });
    }

    private interface Guard extends Thread.UncaughtExceptionHandler {
    }

    /** For bug reports: files/steam-error.txt next to the games. */
    private void record(Throwable error) {
        File root = Library.root(this);
        if (root == null) return;
        try (PrintWriter out = new PrintWriter(new FileWriter(new File(root, "steam-error.txt"), true))) {
            out.println("---- " + new Date());
            error.printStackTrace(out);
        } catch (IOException ignored) {
            // Nothing else to do with it
        }
    }

    private void update(Downloads.State s) {
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (nm != null) nm.notify(NOTIFICATION, notification(s.game.title, s.describe(), Math.round(s.fraction() * 100), s.game));
    }

    private void finished(Downloads.State s) {
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (nm == null) return;
        Notification n = builder()
                .setContentTitle(s.game.title)
                .setContentText(s.error != null ? s.error : App.text(R.string.og_ready))
                .setSmallIcon(android.R.drawable.stat_sys_download_done)
                .setContentIntent(openLauncher())
                .setAutoCancel(true)
                .build();
        nm.notify(NOTIFICATION, n);
    }

    private void startForeground(Notification n) {
        if (Build.VERSION.SDK_INT >= 29) startForeground(NOTIFICATION, n, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        else startForeground(NOTIFICATION, n);
    }

    private Notification.Builder builder() {
        NotificationManager nm = getSystemService(NotificationManager.class);
        if (nm != null && nm.getNotificationChannel(CHANNEL) == null) {
            nm.createNotificationChannel(new NotificationChannel(CHANNEL, App.text(R.string.og_downloads_channel), NotificationManager.IMPORTANCE_LOW));
        }
        return new Notification.Builder(this, CHANNEL);
    }

    private PendingIntent openLauncher() {
        return PendingIntent.getActivity(this, 0, new Intent(this, LauncherActivity.class), PendingIntent.FLAG_IMMUTABLE);
    }

    private Notification notification(String title, String text, int percent, Library.Game game) {
        PendingIntent pause = PendingIntent.getService(this, game.appId, intent(this, ACTION_PAUSE, game),
                PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);
        Notification.Builder b = builder()
                .setContentTitle(title)
                .setContentText(text)
                .setSmallIcon(android.R.drawable.stat_sys_download)
                .setContentIntent(openLauncher())
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .addAction(new Notification.Action.Builder(Icon.createWithResource(this, android.R.drawable.ic_media_pause), "Pause", pause).build());
        if (percent > 0) b.setProgress(100, Math.min(100, percent), false);
        else b.setProgress(0, 0, true);
        return b.build();
    }

    @Override
    public void onDestroy() {
        SteamSession s = session;
        if (s != null) s.cancel();
        super.onDestroy();
    }
}
