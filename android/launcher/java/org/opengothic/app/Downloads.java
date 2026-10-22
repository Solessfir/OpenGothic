package org.opengothic.app;

import java.util.ArrayDeque;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/** Download state per game: DownloadService writes it, the library screen reads it. */
final class Downloads {
    static final class State {
        static final int NONE = 0;
        static final int PAUSE = 1;
        static final int CANCEL = 2;

        final Library.Game game;
        final String language;
        /** Why the player stopped it: PAUSE keeps the files, CANCEL deletes them. */
        volatile int stop = NONE;
        volatile boolean queued = true;
        volatile boolean done;
        volatile String error;
        /** Shown while the size is unknown: connecting, preparing, finishing. */
        volatile String status = "Waiting";
        volatile long bytes;
        volatile long total;
        volatile long speed;
        volatile long eta = -1;
        private final ArrayDeque<long[]> samples = new ArrayDeque<>();

        State(Library.Game game, String language) {
            this.game = game;
            this.language = language;
        }

        boolean active() {
            return !done && error == null;
        }

        float fraction() {
            return total > 0 ? Math.min(1.f, (float) ((double) bytes / total)) : 0.f;
        }

        /** Speed over the last seconds, so a slow chunk does not swing the estimate. */
        synchronized void progress(long done, long size) {
            final long now = System.currentTimeMillis();
            bytes = done;
            total = Math.max(size, done);
            samples.addLast(new long[] {now, done});
            while (samples.size() > 2 && now - samples.peekFirst()[0] > 8000) samples.removeFirst();
            long[] first = samples.peekFirst();
            long span = now - first[0];
            if (span >= 2000 && done >= first[1]) {
                speed = (done - first[1]) * 1000 / span;
                eta = speed > 0 && total > done ? (total - done) / speed : -1;
            }
        }

        /** "412 MB of 2.0 GB · 12.3 MB/s · 4 min left" */
        String describe() {
            if (total <= 0) return status;
            StringBuilder s = new StringBuilder(Library.size(bytes)).append(" of ").append(Library.size(total));
            if (speed > 0) s.append(" · ").append(String.format(Locale.US, "%.1f MB/s", speed / (1024.0 * 1024)));
            if (eta >= 0) s.append(" · ").append(time(eta)).append(" left");
            return s.toString();
        }
    }

    private static final Map<Integer, State> states = new ConcurrentHashMap<>();

    private Downloads() {
    }

    static State get(Library.Game game) {
        return states.get(game.appId);
    }

    static void put(State s) {
        states.put(s.game.appId, s);
    }

    static void forget(Library.Game game) {
        states.remove(game.appId);
    }

    static boolean anyActive() {
        for (State s : states.values()) {
            if (s.active()) return true;
        }
        return false;
    }

    static String time(long seconds) {
        if (seconds >= 3600) return String.format(Locale.US, "%d h %d min", seconds / 3600, seconds / 60 % 60);
        if (seconds >= 60) return String.format(Locale.US, "%d min", seconds / 60);
        return "< 1 min";
    }
}
