package org.opengothic.app;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/** Saves and settings of one game as a ZIP the player keeps outside the app, so they survive uninstalling it. */
final class Backups {
    private static final String MANIFEST = "opengothic-backup.txt";
    private static final Pattern SAVE = Pattern.compile("save_(?:slot_[0-9]+|quick_(?:[1-9]|1[0-9]|20))\\.sav");

    /** A backup of another game; the launcher names it. */
    static final class OtherGame extends IOException {
        final String folder;

        OtherGame(String folder) {
            super("This backup belongs to another game (" + folder + ").");
            this.folder = folder;
        }
    }

    private Backups() {
    }

    /** Saves and ini files; logs are left out. */
    static boolean isUserFile(String name) {
        return SAVE.matcher(name).matches() || name.toLowerCase(Locale.ROOT).endsWith(".ini");
    }

    static List<File> files(File dir) {
        List<File> ret = new ArrayList<>();
        File[] list = dir.listFiles();
        if (list == null) return ret;
        for (File f : list) {
            if (f.isFile() && isUserFile(f.getName())) ret.add(f);
        }
        return ret;
    }

    /** Writes the backup and returns how many files it holds. */
    static int write(File dir, String game, OutputStream target) throws IOException {
        List<File> files = files(dir);
        if (files.isEmpty()) throw new IOException("There are no saves or settings to back up yet.");
        try (ZipOutputStream zip = new ZipOutputStream(target)) {
            zip.putNextEntry(new ZipEntry(MANIFEST));
            zip.write(("game=" + game + "\n").getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();
            byte[] buffer = new byte[64 * 1024];
            for (File f : files) {
                zip.putNextEntry(new ZipEntry(f.getName()));
                try (InputStream in = new FileInputStream(f)) {
                    int count;
                    while ((count = in.read(buffer)) != -1) zip.write(buffer, 0, count);
                }
                zip.closeEntry();
            }
        }
        return files.size();
    }

    /**
     * Restores a backup of the same game: files with the same name are replaced, others stay.
     * Nothing is written unless the whole archive checks out.
     */
    static int restore(InputStream source, File dir, String game) throws IOException {
        File staging = new File(dir.getParentFile(), dir.getName() + ".restore");
        GameFiles.delete(staging);
        if (!staging.mkdirs()) throw new IOException("Cannot prepare the restore.");
        try {
            String owner = null;
            List<String> names = new ArrayList<>();
            try (ZipInputStream zip = new ZipInputStream(source)) {
                ZipEntry entry;
                byte[] buffer = new byte[64 * 1024];
                while ((entry = zip.getNextEntry()) != null) {
                    String name = entry.getName();
                    if (name.equals(MANIFEST)) {
                        BufferedReader reader = new BufferedReader(new InputStreamReader(zip, StandardCharsets.UTF_8));
                        String line = reader.readLine();
                        owner = line != null && line.startsWith("game=") ? line.substring(5).trim() : "";
                        continue;
                    }
                    // Plain file names only, so an archive can never write outside the game's folder
                    if (entry.isDirectory() || !isUserFile(name) || name.contains("/") || name.contains("\\")) {
                        throw new IOException("This is not an OpenGothic backup.");
                    }
                    try (OutputStream out = new FileOutputStream(new File(staging, name))) {
                        int count;
                        while ((count = zip.read(buffer)) != -1) out.write(buffer, 0, count);
                    }
                    names.add(name);
                }
            }
            if (owner == null || names.isEmpty()) throw new IOException("This is not an OpenGothic backup.");
            if (!owner.equals(game)) throw new OtherGame(owner);
            if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create " + dir);
            for (String name : names) {
                File target = new File(dir, name);
                if (target.exists() && !target.delete()) throw new IOException("Cannot replace " + name);
                if (!new File(staging, name).renameTo(target)) throw new IOException("Cannot restore " + name);
            }
            return names.size();
        } finally {
            GameFiles.delete(staging);
        }
    }
}
