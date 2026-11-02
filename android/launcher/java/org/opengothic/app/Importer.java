package org.opengothic.app;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.opengothic.launcher.R;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Imports a game from a ZIP: either a game-data.zip made by the setup scripts, or a plain ZIP of a Windows
 * installation, at any depth. The archive is unpacked into a staging folder, then the launcher works out
 * which game it holds and moves it into that game's folder.
 */
final class Importer {
    static volatile boolean running;
    static volatile int percent;
    static volatile String error;
    /** Folder of the last imported game, so the library can select it. */
    static volatile String imported;
    /** The archive of the last import, offered for deletion once it succeeded. */
    static volatile Uri source;

    private Importer() {
    }

    static void start(Context context, Uri uri) {
        if (running) return;
        final Context app = context.getApplicationContext();
        running = true;
        percent = 0;
        error = null;
        imported = null;
        source = uri;
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
        boolean packaged;
        try (ZipInputStream zip = new ZipInputStream(open(app, uri))) {
            ZipEntry first = zip.getNextEntry();
            packaged = first != null && PrivateAssets.INDEX.equals(first.getName());
        }
        File game;
        if (packaged) {
            try (InputStream in = open(app, uri)) {
                // Interrupted imports continue: files already unpacked are checked instead of copied again
                PrivateAssets.extract(in, staging, marker, (done, total, path) -> percent = (int) (done * 100 / Math.max(1, total)));
            }
            // The package keeps the single-game layout: the game under Gothic2/, settings and saves at the top
            game = new File(staging, "Gothic2");
        } else {
            game = extractPlain(app, uri, staging);
        }
        String folder = GameFiles.detect(game);
        Library.Game g = Library.byFolder(folder);
        if (g == null) throw new IOException(App.text(R.string.og_err_no_game));
        if (Library.installed(root, g)) throw new IOException(g.title + " is already installed. Uninstall it first to import it again.");
        File target = new File(root, g.folder);
        if (target.exists()) GameFiles.delete(target);
        if (!game.renameTo(target)) throw new IOException(App.text(R.string.og_err_move_game));
        if (packaged) GameFiles.moveMissing(staging, Library.user(root, g));
        GameFiles.delete(staging);
        if (marker.exists() && !marker.delete()) marker.deleteOnExit();
        return g.folder;
    }

    private static InputStream open(Context app, Uri uri) throws IOException {
        InputStream in = app.getContentResolver().openInputStream(uri);
        if (in == null) throw new IOException(App.text(R.string.og_err_open_archive));
        return in;
    }

    /**
     * Unpacks what the engine reads from every installation in the archive, wherever it sits: Data, _work and the
     * two ini files next to them. Returns the first one that is a known game, or the staging folder when none is.
     */
    private static File extractPlain(Context app, Uri uri, File staging) throws IOException {
        if (staging.exists()) GameFiles.delete(staging);
        long total = size(app, uri);
        long[] read = {0};
        Map<String, File> installs = new LinkedHashMap<>();
        byte[] buffer = new byte[1024 * 1024];
        try (InputStream raw = open(app, uri);
             ZipInputStream zip = new ZipInputStream(new BufferedInputStream(new FilterInputStream(raw) {
                 @Override
                 public int read(byte[] b, int off, int len) throws IOException {
                     int n = super.read(b, off, len);
                     if (n > 0) read[0] += n;
                     return n;
                 }
             }, 1024 * 1024))) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null; ) {
                if (entry.isDirectory()) continue;
                List<String> parts = Arrays.asList(entry.getName().replace('\\', '/').split("/"));
                if (parts.contains("..")) throw new IOException("Unsafe archive path: " + entry.getName());
                int at = gameSubfolder(parts);
                if (at < 0) continue;
                String prefix = String.join("/", parts.subList(0, at));
                File install = installs.get(prefix);
                if (install == null) installs.put(prefix, install = new File(staging, "game" + installs.size()));
                File target = new File(install, String.join("/", parts.subList(at, parts.size())));
                File parent = target.getParentFile();
                if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create " + parent);
                try (FileOutputStream out = new FileOutputStream(target)) {
                    for (int n; (n = zip.read(buffer)) != -1; ) out.write(buffer, 0, n);
                }
                if (total > 0) percent = (int) Math.min(99, read[0] * 100 / total);
            }
        }
        for (File install : installs.values()) {
            if (!GameFiles.isInstallation(install)) continue;
            GameFiles.prune(install);
            if (GameFiles.detect(install) != null) return install;
        }
        return staging;
    }

    /** Index of the Data or _work folder in a path, or of System for its two ini files; -1 for anything else. */
    private static int gameSubfolder(List<String> parts) {
        for (int i = 0; i < parts.size() - 1; ++i) {
            String name = parts.get(i);
            if (name.isEmpty() || name.equals(".")) return -1;
            if (name.equalsIgnoreCase("Data") || name.equalsIgnoreCase("_work")) return i;
            if (name.equalsIgnoreCase("System")) {
                String file = parts.get(parts.size() - 1);
                boolean ini = i == parts.size() - 2 &&
                        (file.equalsIgnoreCase("GothicGame.ini") || file.equalsIgnoreCase("TheChroniclesOfMyrtana.ini"));
                return ini ? i : -1;
            }
        }
        return -1;
    }

    private static long size(Context app, Uri uri) {
        try (Cursor c = app.getContentResolver().query(uri, new String[] {OpenableColumns.SIZE}, null, null, null)) {
            if (c != null && c.moveToFirst() && !c.isNull(0)) return c.getLong(0);
        } catch (RuntimeException ignored) {
            // Progress just stays at zero
        }
        return 0;
    }
}
