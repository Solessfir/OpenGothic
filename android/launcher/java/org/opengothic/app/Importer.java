package org.opengothic.app;

import android.content.Context;
import android.net.Uri;

import org.opengothic.launcher.R;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

/**
 * Imports a game-data.zip made by the setup scripts. The archive is unpacked into a staging folder,
 * then the launcher works out which game it holds and moves it into that game's folder.
 */
final class Importer {
    static volatile boolean running;
    static volatile int percent;
    static volatile String error;
    /** Folder of the last imported game, so the library can select it. */
    static volatile String imported;

    private Importer() {
    }

    static void start(Context context, Uri uri) {
        if (running) return;
        final Context app = context.getApplicationContext();
        running = true;
        percent = 0;
        error = null;
        imported = null;
        new Thread(() -> {
            try {
                imported = run(app, uri);
            } catch (IOException | RuntimeException e) {
                error = String.valueOf(e.getMessage());
            } finally {
                running = false;
            }
        }, "game-import").start();
    }

    private static String run(Context app, Uri uri) throws IOException {
        File root = Library.root(app);
        if (root == null) throw new IOException(app.getString(R.string.og_storage_unavailable));
        File staging = new File(root, "Import.download");
        File marker = new File(app.getFilesDir(), "import-index.tsv");
        try (InputStream in = app.getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException(App.text(R.string.og_err_open_archive));
            // Interrupted imports continue: files already unpacked are checked instead of copied again
            PrivateAssets.extract(in, staging, marker, (done, total, path) -> percent = (int) (done * 100 / Math.max(1, total)));
        }
        // The package keeps the single-game layout: the game under Gothic2/, settings and saves at the top
        File game = new File(staging, "Gothic2");
        String folder = GameFiles.detect(game);
        Library.Game g = Library.byFolder(folder);
        if (g == null) throw new IOException(App.text(R.string.og_err_no_game));
        if (Library.installed(root, g)) throw new IOException(g.title + " is already installed. Uninstall it first to import it again.");
        File target = new File(root, g.folder);
        if (target.exists()) GameFiles.delete(target);
        if (!game.renameTo(target)) throw new IOException(App.text(R.string.og_err_move_game));
        GameFiles.moveMissing(staging, Library.user(root, g));
        GameFiles.delete(staging);
        if (marker.exists() && !marker.delete()) marker.deleteOnExit();
        return g.folder;
    }
}
