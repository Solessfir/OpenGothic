package org.opengothic.app;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.zip.*;

public class BackupsTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void file(Path path, String content) throws IOException {
        Files.createDirectories(path.getParent());
        Files.write(path, content.getBytes(StandardCharsets.UTF_8));
    }

    private static boolean fails(byte[] zip, File dir, String game) {
        try {
            Backups.restore(new ByteArrayInputStream(zip), dir, game);
            return false;
        } catch (IOException e) {
            return true;
        }
    }

    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        Path user = root.resolve("User/Gothic1");
        file(user.resolve("save_slot_1.sav"), "slot 1");
        file(user.resolve("save_quick_3.sav"), "quick 3");
        file(user.resolve("Gothic.ini"), "settings");
        file(user.resolve("log.txt"), "log");
        file(user.resolve("crash.log"), "crash");

        ByteArrayOutputStream backup = new ByteArrayOutputStream();
        check(Backups.write(user.toFile(), "Gothic1", backup) == 3, "Saves and settings only, no logs");

        Path other = root.resolve("User2/Gothic1");
        file(other.resolve("save_slot_1.sav"), "old slot 1");
        file(other.resolve("save_slot_2.sav"), "slot 2");
        check(Backups.restore(new ByteArrayInputStream(backup.toByteArray()), other.toFile(), "Gothic1") == 3, "Everything restored");
        check(new String(Files.readAllBytes(other.resolve("save_slot_1.sav")), StandardCharsets.UTF_8).equals("slot 1"), "Same slot replaced");
        check(Files.exists(other.resolve("save_slot_2.sav")), "Other slots stay");
        check(Files.exists(other.resolve("Gothic.ini")) && !Files.exists(other.resolve("log.txt")), "Settings without logs");
        check(!Files.exists(root.resolve("User2/Gothic1.restore")), "Staging removed");

        try {
            Backups.restore(new ByteArrayInputStream(backup.toByteArray()), root.resolve("User/Archolos").toFile(), "Archolos");
            check(false, "Backup of another game refused");
        } catch (Backups.OtherGame e) {
            check(e.folder.equals("Gothic1"), "The refusal names the game of the backup");
        }
        check(!Files.exists(root.resolve("User/Archolos/save_slot_1.sav")), "Nothing written for another game");

        ByteArrayOutputStream evil = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(evil)) {
            zip.putNextEntry(new ZipEntry("opengothic-backup.txt"));
            zip.write("game=Gothic1\n".getBytes(StandardCharsets.UTF_8));
            zip.putNextEntry(new ZipEntry("../escape.ini"));
            zip.write(1);
        }
        check(fails(evil.toByteArray(), other.toFile(), "Gothic1"), "Paths are refused");
        check(!Files.exists(root.resolve("User2/escape.ini")), "Nothing escapes the folder");

        check(fails("not a zip".getBytes(StandardCharsets.UTF_8), other.toFile(), "Gothic1"), "Other files are refused");
        check(fails(new byte[0], root.resolve("empty").toFile(), "Gothic1"), "Empty input is refused");
        check(!fails(backup.toByteArray(), root.resolve("User3/Gothic1").toFile(), "Gothic1") && Files.exists(root.resolve("User3/Gothic1/Gothic.ini")),
                "Restore before the game was ever started");
    }
}
