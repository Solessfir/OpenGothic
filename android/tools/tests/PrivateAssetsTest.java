package org.opengothic.app;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.zip.*;

public class PrivateAssetsTest {
    private static String hash(byte[] data) throws Exception {
        StringBuilder result = new StringBuilder();
        for (byte value : MessageDigest.getInstance("SHA-256").digest(data)) result.append(String.format("%02x", value & 255));
        return result.toString();
    }

    private static byte[] archive(String name, byte[] content, boolean corrupt) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ZipOutputStream zip = new ZipOutputStream(bytes)) {
            zip.putNextEntry(new ZipEntry("opengothic-private-v1.tsv"));
            zip.write((content.length + "\t" + hash(content) + "\t" + name + "\n").getBytes(StandardCharsets.UTF_8));
            zip.closeEntry();
            zip.putNextEntry(new ZipEntry(name));
            zip.write(corrupt ? "broken".getBytes(StandardCharsets.UTF_8) : content);
        }
        return bytes.toByteArray();
    }

    private static void extract(byte[] zip, Path root, Path marker) throws Exception {
        PrivateAssets.extract(new ByteArrayInputStream(zip), root.toFile(), marker.toFile(), (done, total, path) -> {});
    }

    private static void fails(byte[] zip, Path root, Path marker) throws Exception {
        try {
            extract(zip, root, marker);
        } catch (IOException expected) {
            return;
        }
        throw new AssertionError("Expected extraction to reject the archive");
    }

    public static void main(String[] args) throws Exception {
        Path base = Paths.get(args[0]);
        Files.createDirectories(base);
        Path root = base.resolve("files"), marker = base.resolve("index");
        if (args.length == 2) {
            long started = System.nanoTime();
            PrivateAssets.extract(new FileInputStream(args[1]), root.toFile(), marker.toFile(), (done, total, path) -> {});
            System.out.println("Real archive extracted and verified in " + ((System.nanoTime() - started) / 1000000000L) + " seconds");
            return;
        }
        byte[] data = "test game data".getBytes(StandardCharsets.UTF_8);
        String archolosConfig = "Gothic2/System/TheChroniclesOfMyrtana.ini";
        extract(archive(archolosConfig, data, false), root, marker);
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve(archolosConfig)), data)) throw new AssertionError("Missing Archolos launcher config");
        Files.delete(marker);
        String name = "Gothic2/Data/Worlds.vdf";
        byte[] zip = archive(name, data, false);
        extract(zip, root, marker);
        java.util.List<InputStream> chunks = new java.util.ArrayList<>();
        for (int offset = 0; offset < zip.length; offset += 13) {
            chunks.add(new ByteArrayInputStream(java.util.Arrays.copyOfRange(zip, offset, Math.min(offset + 13, zip.length))));
        }
        PrivateAssets.extract(new SequenceInputStream(java.util.Collections.enumeration(chunks)),
                base.resolve("chunked").toFile(), base.resolve("chunked-index").toFile(), (done, total, path) -> {});
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve(name)), data)) throw new AssertionError("Wrong extracted data");
        extract(zip, root, marker);
        Files.delete(marker);
        extract(zip, root, marker);
        Files.delete(marker);
        Files.write(root.resolve(name), "user's modified game data".getBytes(StandardCharsets.UTF_8));
        fails(zip, root, marker);
        if (!new String(Files.readAllBytes(root.resolve(name)), StandardCharsets.UTF_8).startsWith("user's")) throw new AssertionError("Overwrote game data");
        fails(archive("Gothic2/Data/../escape", data, false), root, marker);
        fails(archive("/Gothic2/Data/absolute", data, false), root, marker);
        fails(archive("unexpected.exe", data, false), root, marker);
        fails(archive("Gothic2/Data/corrupt.vdf", data, true), root, marker);
        if (Files.exists(root.resolve("Gothic2/Data/corrupt.vdf"))) throw new AssertionError("Published corrupt data");
        extract(archive("Gothic.ini", data, false), root, marker);
        Files.delete(marker);
        extract(archive("Gothic.ini", "different settings".getBytes(StandardCharsets.UTF_8), false), root, marker);
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve("Gothic.ini")), data)) throw new AssertionError("Overwrote settings");
        Files.delete(marker);
        extract(archive("save_slot_1.sav", data, false), root, marker);
        Files.delete(marker);
        extract(archive("save_slot_1.sav", "other save".getBytes(StandardCharsets.UTF_8), false), root, marker);
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve("save_slot_1.sav")), data)) throw new AssertionError("Overwrote save");
        Files.delete(marker);
        extract(archive("save_quick_20.sav", data, false), root, marker);
        Files.delete(marker);
        extract(archive("save_quick_20.sav", "other save".getBytes(StandardCharsets.UTF_8), false), root, marker);
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve("save_quick_20.sav")), data)) throw new AssertionError("Overwrote rotating quicksave");
        Files.delete(marker);
        fails(archive("save_quick_21.sav", data, false), root, marker);
        fails(archive("save_quick_0.sav", data, false), root, marker);
        Files.write(root.resolve("Gothic2/Data/resume.vdf.og-extract-part"), new byte[3]);
        extract(archive("Gothic2/Data/resume.vdf", data, false), root, marker);
        if (!java.util.Arrays.equals(Files.readAllBytes(root.resolve("Gothic2/Data/resume.vdf")), data)) throw new AssertionError("Failed to resume");
        System.out.println("PrivateAssets checks passed: extraction, repeat launch, interrupted setup, conflicts, path safety, integrity, preserved settings/saves.");
    }
}
