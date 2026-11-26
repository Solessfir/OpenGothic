package org.opengothic.app;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;

import org.opengothic.launcher.R;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FilterInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Imports a game from a ZIP: either a game-data.zip made by the setup scripts, or a plain ZIP of a Windows
 * installation, at any depth, or a folder with such an installation. What the engine reads is copied into a
 * staging folder, then the launcher works out which game it holds and moves it into that game's folder.
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

    /** The folder grant of an import killed with the process is never released by the import itself. */
    static void releaseStaleGrants(Context context) {
        if (running) return;
        String tree = context.getSharedPreferences("launcher", Context.MODE_PRIVATE).getString(GRANT, null);
        if (tree != null) releaseGrant(context, Uri.parse(tree));
    }

    private static final String GRANT = "import_tree";

    private static void releaseGrant(Context app, Uri tree) {
        try {
            app.getContentResolver().releasePersistableUriPermission(tree, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {
            // The grant was never taken or is gone already
        }
        app.getSharedPreferences("launcher", Context.MODE_PRIVATE).edit().remove(GRANT).apply();
    }

    static void start(Context context, Uri uri, boolean folder) {
        if (running) return;
        final Context app = context.getApplicationContext();
        if (folder) {
            // The copy opens every file long after the picker returns, and the plain grant ends with the activity
            try {
                app.getContentResolver().takePersistableUriPermission(uri, android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION);
                app.getSharedPreferences("launcher", Context.MODE_PRIVATE).edit().putString(GRANT, uri.toString()).apply();
            } catch (SecurityException ignored) {
                // The plain grant still covers an import that finishes while the launcher is open
            }
        }
        running = true;
        percent = 0;
        error = null;
        imported = null;
        // A picked folder is not offered for deletion: it is usually the installation the player keeps
        source = folder ? null : uri;
        new Thread(() -> {
            try {
                imported = run(app, uri, folder);
            } catch (IOException | RuntimeException e) {
                error = String.valueOf(e.getMessage());
            } finally {
                if (folder) releaseGrant(app, uri);
                running = false;
            }
        }, "game-import").start();
    }

    private static String run(Context app, Uri uri, boolean folder) throws IOException {
        File root = Library.root(app);
        if (root == null) throw new IOException(app.getString(R.string.og_storage_unavailable));
        File staging = new File(root, "Import.download");
        File marker = new File(app.getFilesDir(), "import-index.tsv");
        boolean packaged = false;
        if (!folder) {
            try (ZipInputStream zip = new ZipInputStream(open(app, uri))) {
                ZipEntry first = zip.getNextEntry();
                packaged = first != null && PrivateAssets.INDEX.equals(first.getName());
            }
        }
        try {
            return install(app, uri, folder, packaged, root, staging, marker);
        } catch (IOException | RuntimeException e) {
            // Only a packaged import resumes; a failed copy would otherwise keep gigabytes until the next import
            if (!packaged) {
                try {
                    // The marker belongs to an earlier packaged import whose staging is gone now
                    if (marker.exists() && !marker.delete()) marker.deleteOnExit();
                    GameFiles.delete(staging);
                } catch (IOException ignored) {
                    // The next import clears it
                }
            }
            throw e;
        }
    }

    private static String install(Context app, Uri uri, boolean folder, boolean packaged, File root, File staging, File marker) throws IOException {
        File game;
        if (folder) {
            game = copyFolder(app, uri, staging);
        } else if (packaged) {
            try (InputStream in = open(app, uri)) {
                // Interrupted imports continue: files already unpacked are checked instead of copied again
                PrivateAssets.extract(in, staging, marker, (done, total, path) -> percent = (int) (done * 100 / Math.max(1, total)));
            }
            // The package keeps the single-game layout: the game under Gothic2/, settings and saves at the top
            game = new File(staging, "Gothic2");
        } else {
            game = extractPlain(app, uri, staging);
        }
        Library.Game g = Library.byFolder(GameFiles.detect(game));
        if (g == null) throw new IOException(App.text(R.string.og_err_no_game));
        if (Library.installed(root, g)) throw new IOException(g.title + " is already installed. Uninstall it first to import it again.");
        File target = new File(root, g.folder);
        if (target.exists()) GameFiles.delete(target);
        if (!game.renameTo(target)) throw new IOException(App.text(R.string.og_err_move_game));
        if (packaged) GameFiles.moveMissing(staging, Library.user(root, g));
        try {
            GameFiles.delete(staging);
        } catch (IOException ignored) {
            // The game is installed; the next import clears what is left
        }
        if (marker.exists() && !marker.delete()) marker.deleteOnExit();
        return g.folder;
    }

    private static InputStream open(Context app, Uri uri) throws IOException {
        return open(app, uri, R.string.og_err_open_archive);
    }

    private static InputStream open(Context app, Uri uri, int error) throws IOException {
        InputStream in = app.getContentResolver().openInputStream(uri);
        if (in == null) throw new IOException(App.text(error));
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
                File target = target(installs, staging, parts);
                if (target == null) continue;
                try (FileOutputStream out = new FileOutputStream(target)) {
                    for (int n; (n = zip.read(buffer)) != -1; ) out.write(buffer, 0, n);
                }
                if (total > 0) percent = (int) Math.min(99, read[0] * 100 / total);
            }
        }
        return pickInstall(installs, staging, Library.root(app));
    }

    /** A file in the picked folder: its document and its path below that folder. */
    private static final class Entry {
        final String id;
        final List<String> parts;
        final long size;

        Entry(String id, List<String> parts, long size) {
            this.id = id;
            this.parts = parts;
            this.size = size;
        }
    }

    /** Copies what the engine reads from every installation in a picked folder, as extractPlain does for a ZIP. */
    private static File copyFolder(Context app, Uri tree, File staging) throws IOException {
        if (staging.exists()) GameFiles.delete(staging);
        List<Entry> files = new ArrayList<>();
        list(app, tree, DocumentsContract.getTreeDocumentId(tree), new ArrayList<>(), files);
        Map<String, List<Entry>> byInstall = new LinkedHashMap<>();
        for (Entry f : files) {
            String prefix = String.join("/", f.parts.subList(0, gameSubfolder(f.parts)));
            List<Entry> install = byInstall.get(prefix);
            if (install == null) byInstall.put(prefix, install = new ArrayList<>());
            install.add(f);
        }
        // A broad pick can hold several copies of a game and other apps' Data folders: detection needs only the ini
        // files and the world catalogs, so only the installation that will be imported is copied in full
        File root = Library.root(app);
        List<Entry> chosen = null;
        List<Entry> installed = null;
        File probe = new File(staging, "probe");
        for (List<Entry> install : byInstall.values()) {
            if (!hasDataAndWork(install)) continue;
            for (Entry f : install) {
                if (isProbeFile(f)) copy(app, tree, f, probe, null);
            }
            Library.Game g;
            try {
                g = Library.byFolder(GameFiles.detect(probe));
            } catch (IOException e) {
                // A damaged world catalog in one folder must not stop the import of another
                g = null;
            }
            GameFiles.delete(probe);
            if (g == null) continue;
            if (root == null || !Library.installed(root, g)) {
                chosen = install;
                break;
            }
            if (installed == null) installed = install;
        }
        if (chosen == null) chosen = installed;
        if (chosen == null) return staging;
        long[] progress = {0, 0};
        for (Entry f : chosen) progress[1] += f.size;
        File game = new File(staging, "game0");
        for (Entry f : chosen) copy(app, tree, f, game, progress);
        GameFiles.prune(game);
        return game;
    }

    private static boolean hasDataAndWork(List<Entry> install) {
        boolean data = false, work = false;
        for (Entry f : install) {
            String top = f.parts.get(gameSubfolder(f.parts));
            data |= top.equalsIgnoreCase("Data");
            work |= top.equalsIgnoreCase("_work");
        }
        return data && work;
    }

    /** The files GameFiles.detect reads: the ini files in System and the world catalogs directly in Data. */
    private static boolean isProbeFile(Entry f) {
        int at = gameSubfolder(f.parts);
        if (f.parts.get(at).equalsIgnoreCase("System")) return true;
        String file = f.parts.get(f.parts.size() - 1);
        return f.parts.size() == at + 2 && f.parts.get(at).equalsIgnoreCase("Data") &&
                (file.equalsIgnoreCase("Worlds.vdf") || file.equalsIgnoreCase("Worlds_Addon.vdf"));
    }

    /** Copies a file of the picked folder below base, at its path within the installation. */
    private static void copy(Context app, Uri tree, Entry f, File base, long[] progress) throws IOException {
        checkPath(f.parts);
        File target = new File(base, String.join("/", f.parts.subList(gameSubfolder(f.parts), f.parts.size())));
        File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create " + parent);
        byte[] buffer = new byte[1024 * 1024];
        try (InputStream in = open(app, DocumentsContract.buildDocumentUriUsingTree(tree, f.id), R.string.og_err_open_folder);
             FileOutputStream out = new FileOutputStream(target)) {
            for (int n; (n = in.read(buffer)) != -1; ) {
                out.write(buffer, 0, n);
                if (progress == null) continue;
                progress[0] += n;
                if (progress[1] > 0) percent = (int) Math.min(99, progress[0] * 100 / progress[1]);
            }
        }
    }

    /** Collects the files below a folder that the engine reads, searching every subfolder for installations. */
    private static void list(Context app, Uri tree, String parent, List<String> path, List<Entry> out) throws IOException {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parent);
        String[] columns = {DocumentsContract.Document.COLUMN_DOCUMENT_ID, DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE};
        try (Cursor c = app.getContentResolver().query(children, columns, null, null, DocumentsContract.Document.COLUMN_DISPLAY_NAME)) {
            if (c == null) throw new IOException(App.text(R.string.og_err_open_folder));
            while (c.moveToNext()) {
                String name = c.getString(1);
                if (name == null) continue;
                List<String> parts = new ArrayList<>(path);
                parts.add(name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(2))) {
                    // The launcher's own library holds installed games; a pick of the storage root would find those first
                    if (!name.equals(app.getPackageName())) list(app, tree, c.getString(0), parts, out);
                } else if (gameSubfolder(parts) >= 0) {
                    out.add(new Entry(c.getString(0), parts, c.isNull(3) ? 0 : c.getLong(3)));
                }
            }
        }
    }

    /** Names from an archive or a storage provider must not lead out of staging. */
    private static void checkPath(List<String> parts) throws IOException {
        for (String part : parts)
            if (part.equals("..") || part.contains("/") || part.contains("\\")) throw new IOException("Unsafe path: " + String.join("/", parts));
    }

    /** Where a file goes in staging, one folder per installation; null for files the engine does not read. */
    private static File target(Map<String, File> installs, File staging, List<String> parts) throws IOException {
        int at = gameSubfolder(parts);
        if (at < 0) return null;
        checkPath(parts);
        String prefix = String.join("/", parts.subList(0, at));
        File install = installs.get(prefix);
        if (install == null) installs.put(prefix, install = new File(staging, "game" + installs.size()));
        File target = new File(install, String.join("/", parts.subList(at, parts.size())));
        File parent = target.getParentFile();
        if (!parent.isDirectory() && !parent.mkdirs()) throw new IOException("Cannot create " + parent);
        return target;
    }

    /**
     * The first copied installation of a known game that is not installed yet. When all of them are installed, the
     * first one, so the import reports that; the staging folder when none is a known game.
     */
    private static File pickInstall(Map<String, File> installs, File staging, File root) throws IOException {
        File known = null;
        for (File install : installs.values()) {
            if (!GameFiles.isInstallation(install)) continue;
            GameFiles.prune(install);
            Library.Game g = Library.byFolder(GameFiles.detect(install));
            if (g == null) continue;
            if (root == null || !Library.installed(root, g)) return install;
            if (known == null) known = install;
        }
        return known != null ? known : staging;
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
