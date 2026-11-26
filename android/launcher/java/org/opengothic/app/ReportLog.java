package org.opengothic.app;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

import java.util.LinkedHashSet;
import java.util.Set;

final class ReportLog {
    private ReportLog() {
    }

    /** Shaders whose GPU time differs a lot between phones; the driver's statistics for them show why. */
    private static final String[] STATS_SHADERS = {"fog.frag ", "sun_shafts.comp ", "visibility_pass.comp "};

    /** The driver's compiler statistics for a few shaders, logged at the start of each game run. */
    static String shaderStats(File file) throws IOException {
        if (!file.isFile()) return null;
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            byte[] data = new byte[(int) Math.min(in.length(), 1024 * 1024)];
            in.readFully(data);
            Set<String> found = new LinkedHashSet<>();
            try (BufferedReader lines = new BufferedReader(new InputStreamReader(new ByteArrayInputStream(data), StandardCharsets.UTF_8))) {
                for (String line; (line = lines.readLine()) != null; ) {
                    if (!line.startsWith("Shader stats ")) continue;
                    for (String s : STATS_SHADERS) {
                        if (line.startsWith(s, "Shader stats ".length()) && line.length() < 1000) found.add(line);
                    }
                }
            }
            StringBuilder result = new StringBuilder();
            for (String line : found) {
                if (result.length() + line.length() + 1 > 4000) break;
                result.append(line).append('\n');
            }
            return result.toString();
        }
    }

    static String diagnostics(File file) throws IOException {
        if (!file.isFile()) return null;
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            // shortcut: Only scan the last MiB; use a dedicated diagnostics log if warnings exceed that between samples.
            long length = in.length();
            long start = Math.max(0, length - 1024 * 1024);
            byte[] data = new byte[(int) (length - start)];
            boolean partial = false;
            if (start > 0) {
                in.seek(start - 1);
                partial = in.read() != '\n';
            }
            in.seek(start);
            in.readFully(data);
            StringBuilder result = new StringBuilder();
            try (BufferedReader lines = new BufferedReader(new InputStreamReader(new ByteArrayInputStream(data), StandardCharsets.UTF_8))) {
                if (partial) lines.readLine();
                for (String line; (line = lines.readLine()) != null; ) {
                    if (!line.startsWith("Status:") && !line.startsWith("Frame timings:") && !line.startsWith("Graphics:") && !line.startsWith("Memory ")) continue;
                    if (line.length() >= 4000) continue;
                    while (result.length() + line.length() + 1 > 4000) result.delete(0, result.indexOf("\n") + 1);
                    result.append(line).append('\n');
                }
            }
            return result.toString();
        }
    }
}
