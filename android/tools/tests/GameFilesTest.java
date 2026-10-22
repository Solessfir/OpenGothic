package org.opengothic.app;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;

public class GameFilesTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void file(Path path, byte[] content) throws IOException {
        Files.createDirectories(path.getParent());
        Files.write(path, content);
    }

    /** A VDF catalog with one world entry and one directory entry, the layout GameFiles.worldNames reads. */
    private static byte[] vdf(String world) {
        byte[] data = new byte[296 + 2 * 80];
        byte[] signature = "PSVDSC_V2.00\r\n\r\n".getBytes(StandardCharsets.ISO_8859_1);
        System.arraycopy(signature, 0, data, 256, signature.length);
        data[272] = 2;
        data[289] = 1; // catalog offset 296
        data[288] = 40;
        String[] names = {"WORLDS", world};
        for (int i = 0; i < 2; ++i) {
            int at = 296 + i * 80;
            byte[] name = String.format("%-64s", names[i]).getBytes(StandardCharsets.ISO_8859_1);
            System.arraycopy(name, 0, data, at, 64);
            if (i == 0) data[at + 75] = (byte) 0x80;
        }
        return data;
    }

    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        Path game = root.resolve("game");
        file(game.resolve("DATA/Worlds.vdf"), vdf("WORLD.ZEN"));
        file(game.resolve("_work/Data/Scripts/_compiled/GOTHIC.DAT"), new byte[] {1});
        file(game.resolve("_work/tools/plugin.dll"), new byte[] {1});
        file(game.resolve("System/GothicGame.ini"), new byte[] {1});
        file(game.resolve("System/Gothic.exe"), new byte[] {1});
        file(game.resolve("System/Gothic.ini"), new byte[] {1});
        file(game.resolve("Saves/savegame1/SAVEDAT.SAV"), new byte[] {1});
        file(game.resolve("manual.pdf"), new byte[] {1});

        check(GameFiles.isInstallation(game.toFile()), "Data and _work are matched in any case");
        check("Gothic1".equals(GameFiles.detect(game.toFile())), "WORLD.ZEN is Gothic 1");
        GameFiles.prune(game.toFile());
        check(Files.exists(game.resolve("DATA/Worlds.vdf")), "Data stays");
        check(Files.exists(game.resolve("_work/Data/Scripts/_compiled/GOTHIC.DAT")), "Scripts stay");
        check(Files.exists(game.resolve("System/GothicGame.ini")), "The game ini stays");
        check(!Files.exists(game.resolve("_work/tools/plugin.dll")), "Windows libraries go");
        check(!Files.exists(game.resolve("System/Gothic.exe")), "Executables go");
        check(!Files.exists(game.resolve("System/Gothic.ini")), "Desktop settings go");
        check(!Files.exists(game.resolve("Saves")), "Original saves go");
        check(!Files.exists(game.resolve("manual.pdf")), "Everything else goes");

        Path notr = root.resolve("notr");
        file(notr.resolve("Data/Worlds_Addon.vdf"), vdf("ADDONWORLD.ZEN"));
        Files.createDirectories(notr.resolve("_work"));
        check("Gothic2".equals(GameFiles.detect(notr.toFile())), "The addon world is Gothic II");
        file(notr.resolve("system/TheChroniclesOfMyrtana.ini"), new byte[] {1});
        check("Archolos".equals(GameFiles.detect(notr.toFile())), "The Archolos ini wins over the worlds");

        Path empty = root.resolve("empty");
        Files.createDirectories(empty.resolve("Data"));
        check(GameFiles.detect(empty.toFile()) == null, "No worlds, no game");
    }
}
