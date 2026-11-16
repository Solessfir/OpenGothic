package org.opengothic.app;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;

final class ReportLog {
    private ReportLog() {
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
