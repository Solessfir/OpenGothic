package org.opengothic.app;

import android.content.Context;

import java.io.File;
import java.util.Locale;

/** The games of the launcher, where each one lives and what Steam calls it. */
final class Library {
    static final class Game {
        /** Folder in the app's external files and the name written to game.txt for the engine. */
        final String folder;
        final String name;
        final String title;
        final int appId;
        /** Steam grants the license on request. */
        final boolean free;

        Game(String folder, String name, String title, int appId, boolean free) {
            this.folder = folder;
            this.name = name;
            this.title = title;
            this.appId = appId;
            this.free = free;
        }
    }

    static final Game[] GAMES = {
        new Game("Gothic1", "Gothic", "Gothic", 65540, false),
        new Game("Gothic2", "Gothic II", "Gothic II: Gold Edition", 39510, false),
        new Game("Archolos", "Archolos", "The Chronicles of Myrtana: Archolos", 1467450, true),
    };

    private Library() {
    }

    static Game byFolder(String folder) {
        for (Game g : GAMES) {
            if (g.folder.equals(folder)) return g;
        }
        return null;
    }

    static Game byApp(int appId) {
        for (Game g : GAMES) {
            if (g.appId == appId) return g;
        }
        return null;
    }

    static File root(Context context) {
        return context.getExternalFilesDir(null);
    }

    static boolean installed(File root, Game game) {
        return root != null && GameFiles.isInstallation(new File(root, game.folder));
    }

    /** Download in progress or paused; DepotDownloader resumes from it. */
    static File staging(File root, Game game) {
        return new File(root, game.folder + ".download");
    }

    static File user(File root, Game game) {
        return new File(new File(root, "User"), game.folder);
    }

    /** Steam's language names, shown in the language itself. */
    static String languageName(String steam) {
        switch (steam) {
            case "english": return "English";
            case "german": return "Deutsch";
            case "polish": return "Polski";
            case "russian": return "Русский";
            case "french": return "Français";
            case "italian": return "Italiano";
            case "spanish": return "Español";
            case "czech": return "Čeština";
            case "hungarian": return "Magyar";
            case "ukrainian": return "Українська";
            default: return steam.isEmpty() ? steam : Character.toUpperCase(steam.charAt(0)) + steam.substring(1);
        }
    }

    /** The Steam language matching the phone, so the picker can preselect it. */
    static String phoneLanguage() {
        switch (Locale.getDefault().getLanguage()) {
            case "de": return "german";
            case "pl": return "polish";
            case "ru": return "russian";
            case "fr": return "french";
            case "it": return "italian";
            case "es": return "spanish";
            case "cs": return "czech";
            case "hu": return "hungarian";
            case "uk": return "ukrainian";
            default: return "english";
        }
    }

    static String size(long bytes) {
        if (bytes >= 1024L * 1024 * 1024) return String.format(Locale.US, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
        return String.format(Locale.US, "%d MB", Math.max(1, bytes / (1024 * 1024)));
    }
}
