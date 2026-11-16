package org.opengothic.app;

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;

public class ReportLogTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static String diagnostics(Path file, String content) throws Exception {
        Files.write(file, content.getBytes(StandardCharsets.UTF_8));
        return ReportLog.diagnostics(file.toFile());
    }

    public static void main(String[] args) throws Exception {
        Path root = Paths.get(args[0]);
        Files.createDirectories(root);
        Path log = root.resolve("log.txt");
        check(ReportLog.diagnostics(new File(root.toFile(), "missing.txt")) == null, "Missing log is omitted");

        String samples = "Graphics: Mali\nStatus: fps=9\nFrame timings: CPU=40\nMemory after load or save: process MB=1257\n";
        StringBuilder spam = new StringBuilder(samples);
        for (int i = 0; i < 10000; ++i) spam.append("random animation R_ROAM1 is not played\n");
        check(diagnostics(log, spam.toString()).equals(samples), "Warnings do not displace diagnostics before the ordinary tail");
        check(diagnostics(log, "warning\n").isEmpty(), "A log without diagnostics adds no section");

        String recent = "\nMemory recent: complete\n";
        StringBuilder cutoff = new StringBuilder("Status: partial\n");
        while (cutoff.length() < 1024 * 1024 - recent.length()) cutoff.append('x');
        cutoff.append(recent);
        check(diagnostics(log, "Status: outside scan\nignored " + cutoff).equals("Memory recent: complete\n"), "The scan is capped and a partial first line cannot become a diagnostic");
        check(diagnostics(log, "warning before boundary\n" + cutoff).equals("Status: partial\nMemory recent: complete\n"), "A diagnostic exactly at the scan boundary is retained");

        StringBuilder newest = new StringBuilder("Status: oldest\n");
        for (int i = 0; i < 200; ++i) newest.append("Status: sample ").append(i).append(" сохранено 🐈\n");
        String result = diagnostics(log, newest.toString());
        check(result.length() <= 4000 && !result.contains("oldest") && !result.contains("sample 0 "), "Only the newest bounded diagnostics survive");
        check(result.startsWith("Status:") && result.endsWith("Status: sample 199 сохранено 🐈\n"), "Complete UTF-8 diagnostic lines are retained");
        check(diagnostics(log, newest.append("Graphics: ").append(new String(new char[4000]).replace('\0', 'x')).append('\n').append("Status: final\n").toString()).endsWith("Status: final\n"), "An oversized diagnostic cannot grow the result");
    }
}
