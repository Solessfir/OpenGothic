package org.opengothic.app;

import java.io.File;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Game installations on the phone: what OpenGothic reads, which game a folder holds, removal. */
final class GameFiles {
    // Same rules as the ZIP import (android/tools/game_package.py): the engine reads Data, _work and two ini files
    private static final String[] BLOCKED = {".exe", ".dll", ".bat", ".cmd", ".ps1", ".lnk", ".sav", ".log"};
    private static final String[] SYSTEM_FILES = {"gothicgame.ini", "thechroniclesofmyrtana.ini"};

    private GameFiles() {
    }

    /** Child by name, ignoring case like the engine does: Windows installations mix "Data", "DATA" and "data". */
    static File child(File dir, String name) {
        File[] list = dir == null ? null : dir.listFiles();
        if (list == null) return null;
        for (File f : list) {
            if (f.getName().equalsIgnoreCase(name)) return f;
        }
        return null;
    }

    static boolean isInstallation(File dir) {
        File data = child(dir, "Data");
        File work = child(dir, "_work");
        return data != null && data.isDirectory() && work != null && work.isDirectory();
    }

    /**
     * The engine refuses to start without _work/Data/Scripts/_compiled, even for a mod that keeps its scripts in archives.
     * Steam downloads of Archolos can lack that folder, so it is created when missing.
     */
    static void ensureScriptsFolder(File game) throws IOException {
        File dir = game;
        for (String name : new String[] {"_work", "Data", "Scripts", "_compiled"}) {
            File next = child(dir, name);
            dir = next != null ? next : new File(dir, name);
        }
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("Cannot create " + dir);
    }

    /** Archives that do not start like a VDF, such as files an interrupted download left zeroed. */
    static List<String> damaged(File game) {
        List<String> ret = new ArrayList<>();
        File data = child(game, "Data");
        if (data != null) damaged(data, ret);
        return ret;
    }

    private static void damaged(File dir, List<String> ret) {
        File[] list = dir.listFiles();
        if (list == null) return;
        for (File f : list) {
            if (f.isDirectory()) {
                damaged(f, ret);
                continue;
            }
            String name = f.getName().toLowerCase(Locale.ROOT);
            // The engine skips empty archives, and anything shorter cannot hold the header
            if (!(name.endsWith(".vdf") || name.endsWith(".mod")) || f.length() < 296) continue;
            try (RandomAccessFile in = new RandomAccessFile(f, "r")) {
                byte[] signature = new byte[12];
                in.seek(256);
                in.readFully(signature);
                if (!new String(signature, StandardCharsets.ISO_8859_1).equals("PSVDSC_V2.00")) ret.add(f.getName());
            } catch (IOException e) {
                ret.add(f.getName());
            }
        }
    }

    /** Removes everything a Windows installation has that the engine never reads. */
    static void prune(File root) throws IOException {
        File[] list = root.listFiles();
        if (list == null) throw new IOException("Cannot read " + root);
        for (File f : list) {
            String name = f.getName().toLowerCase(Locale.ROOT);
            if ((name.equals("data") || name.equals("_work")) && f.isDirectory()) {
                pruneTree(f);
            } else if (name.equals("system") && f.isDirectory()) {
                File[] system = f.listFiles();
                if (system == null) continue;
                for (File s : system) {
                    if (!s.isFile() || !contains(SYSTEM_FILES, s.getName().toLowerCase(Locale.ROOT))) delete(s);
                }
            } else {
                delete(f);
            }
        }
    }

    private static void pruneTree(File dir) throws IOException {
        File[] list = dir.listFiles();
        if (list == null) return;
        for (File f : list) {
            if (f.isDirectory()) {
                pruneTree(f);
            } else {
                String name = f.getName().toLowerCase(Locale.ROOT);
                for (String suffix : BLOCKED) {
                    if (name.endsWith(suffix)) {
                        delete(f);
                        break;
                    }
                }
            }
        }
    }

    /** The launcher folder of the game in an installation, or null when it is none of the three. */
    static String detect(File root) throws IOException {
        File system = child(root, "System");
        File archolos = child(system, "TheChroniclesOfMyrtana.ini");
        if (archolos != null && archolos.isFile()) return "Archolos";
        File data = child(root, "Data");
        Set<String> worlds = new HashSet<>();
        File worldsVdf = child(data, "Worlds.vdf");
        if (worldsVdf != null && worldsVdf.isFile()) worlds.addAll(worldNames(worldsVdf));
        File addon = child(data, "Worlds_Addon.vdf");
        if (addon != null && addon.isFile()) worlds.addAll(worldNames(addon));
        if (worlds.contains("WORLD.ZEN")) return "Gothic1";
        if (worlds.contains("NEWWORLD.ZEN") || worlds.contains("ADDONWORLD.ZEN")) return "Gothic2";
        return null;
    }

    /** World names from a VDF catalog, the layout ZenKit's Vfs.cc reads. */
    static Set<String> worldNames(File vdf) throws IOException {
        Set<String> names = new HashSet<>();
        try (RandomAccessFile in = new RandomAccessFile(vdf, "r")) {
            byte[] header = new byte[296];
            in.readFully(header);
            String signature = new String(header, 256, 12, StandardCharsets.ISO_8859_1);
            if (!signature.equals("PSVDSC_V2.00")) throw new IOException("Invalid world archive: " + vdf.getName());
            long count = u32(header, 272);
            long offset = u32(header, 288);
            if (offset == 0) offset = 296;
            if (offset < 296 || count > 1000000 || offset + count * 80 > in.length()) {
                throw new IOException("Invalid world catalog: " + vdf.getName());
            }
            in.seek(offset);
            byte[] entry = new byte[80];
            for (long i = 0; i < count; ++i) {
                in.readFully(entry);
                if ((u32(entry, 72) & 0x80000000L) != 0) continue;
                int end = 0;
                while (end < 64 && entry[end] != 0) ++end;
                String name = new String(entry, 0, end, StandardCharsets.ISO_8859_1).trim().toUpperCase(Locale.ROOT);
                if (name.endsWith(".ZEN")) names.add(name);
            }
        }
        return names;
    }

    private static long u32(byte[] b, int at) {
        return (b[at] & 0xFFL) | (b[at + 1] & 0xFFL) << 8 | (b[at + 2] & 0xFFL) << 16 | (b[at + 3] & 0xFFL) << 24;
    }

    private static boolean contains(String[] list, String value) {
        for (String s : list) {
            if (s.equals(value)) return true;
        }
        return false;
    }

    static void delete(File f) throws IOException {
        File[] list = f.listFiles();
        if (list != null) {
            for (File c : list) delete(c);
        }
        if (f.exists() && !f.delete()) throw new IOException("Cannot delete " + f);
    }

    /** Moves the files of a directory into another one, leaving files that already exist there. */
    static void moveMissing(File from, File to) throws IOException {
        File[] list = from.listFiles();
        if (list == null) return;
        if (!to.isDirectory() && !to.mkdirs()) throw new IOException("Cannot create " + to);
        for (File f : list) {
            File target = new File(to, f.getName());
            if (f.isFile() && !target.exists() && !f.renameTo(target)) throw new IOException("Cannot move " + f.getName());
        }
    }
}
