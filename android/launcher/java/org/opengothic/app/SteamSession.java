package org.opengothic.app;

import android.annotation.TargetApi;
import android.util.Log;

import org.bouncycastle.jce.provider.BouncyCastleProvider;

import java.io.Closeable;
import java.io.File;
import java.io.IOException;
import java.security.SecureRandom;
import java.security.Security;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

import in.dragonbra.javasteam.depotdownloader.DepotDownloader;
import in.dragonbra.javasteam.depotdownloader.IDownloadListener;
import in.dragonbra.javasteam.depotdownloader.data.AppItem;
import in.dragonbra.javasteam.depotdownloader.data.DownloadItem;
import in.dragonbra.javasteam.enums.EResult;
import in.dragonbra.javasteam.protobufs.steamclient.SteammessagesAuthSteamclient.EAuthSessionGuardType;
import in.dragonbra.javasteam.steam.authentication.AuthPollResult;
import in.dragonbra.javasteam.steam.authentication.AuthSessionDetails;
import in.dragonbra.javasteam.steam.authentication.AuthenticationException;
import in.dragonbra.javasteam.steam.authentication.CredentialsAuthSession;
import in.dragonbra.javasteam.steam.authentication.IAuthenticator;
import in.dragonbra.javasteam.steam.authentication.QrAuthSession;
import in.dragonbra.javasteam.steam.handlers.steamapps.License;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSProductInfo;
import in.dragonbra.javasteam.steam.handlers.steamapps.PICSRequest;
import in.dragonbra.javasteam.steam.handlers.steamapps.SteamApps;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.FreeLicenseCallback;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.LicenseListCallback;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.PICSProductInfoCallback;
import in.dragonbra.javasteam.steam.handlers.steamfriends.SteamFriends;
import in.dragonbra.javasteam.steam.handlers.steamfriends.callback.PersonaStateCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.LogOnDetails;
import in.dragonbra.javasteam.steam.handlers.steamuser.SteamUser;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOffCallback;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback;
import in.dragonbra.javasteam.steam.steamclient.SteamClient;
import in.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager;
import in.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.callbacks.DisconnectedCallback;
import in.dragonbra.javasteam.types.AsyncJobMultiple;
import in.dragonbra.javasteam.types.KeyValue;
import in.dragonbra.javasteam.types.SteamID;

/**
 * One connection to Steam through JavaSteam (MIT): sign-in, the account's games and game downloads.
 * It signs in as the Steam client, the only kind of session allowed to download content.
 * Every method blocks; call them from a worker thread. JavaSteam needs Android 8 (java.time).
 */
@TargetApi(26)
final class SteamSession implements Closeable {
    private static final String TAG = "OpenGothic/Steam";

    static {
        // JavaSteam asks for the "BC" provider by name. Android ships a stripped one under that name, so replace it.
        Security.removeProvider("BC");
        Security.insertProviderAt(new BouncyCastleProvider(), 1);
    }

    /** Questions for the player; answers may come from any thread. */
    interface Prompt {
        /** Steam Guard code from the Steam app or e-mail. Complete with null to give up. */
        void code(boolean email, String emailHint, boolean wrong, CompletableFuture<String> answer);

        /** Steam waits for the sign-in to be approved in the Steam app. */
        void approve();

        /** QR sign-in: the link to show as a code, changes every few seconds. */
        void qr(String url);
    }

    interface Progress {
        void status(String text);

        /** Bytes done and the size of the whole download, 0 while unknown. */
        void bytes(long done, long total);
    }

    static final class Failure extends Exception {
        /** The session ended or was refused: sign in again. */
        final boolean signIn;

        Failure(String message, boolean signIn) {
            super(message);
            this.signIn = signIn;
        }
    }

    private final SteamClient client;
    private final CallbackManager manager;
    private final SteamUser user;
    private final SteamApps apps;
    private final SteamFriends friends;
    private final List<Closeable> subscriptions = new ArrayList<>();

    private volatile boolean running = true;
    private volatile boolean closing;

    private volatile CompletableFuture<Void> connected = new CompletableFuture<>();
    private volatile CompletableFuture<LoggedOnCallback> loggedOn = new CompletableFuture<>();
    private volatile CompletableFuture<List<License>> licenses = new CompletableFuture<>();
    private volatile DepotDownloader downloader;
    private volatile CredentialsAuthSession credentials;
    private volatile boolean emailCode;
    private volatile Future<?> pending;
    private volatile CompletableFuture<PersonaStateCallback> profile;
    private volatile SteamID me;

    SteamSession() {
        client = new SteamClient();
        manager = new CallbackManager(client);
        user = client.getHandler(SteamUser.class);
        apps = client.getHandler(SteamApps.class);
        friends = client.getHandler(SteamFriends.class);

        subscriptions.add(manager.subscribe(ConnectedCallback.class, c -> connected.complete(null)));
        subscriptions.add(manager.subscribe(DisconnectedCallback.class, c -> {
            if (!closing && !c.isUserInitiated()) failAll(new Failure("The connection to Steam was lost.", false));
        }));
        subscriptions.add(manager.subscribe(LoggedOnCallback.class, c -> loggedOn.complete(c)));
        subscriptions.add(manager.subscribe(LoggedOffCallback.class, c -> {
            // Only a token that stopped working means signing in again; another session of the account taking over does not
            Log.w(TAG, "logged off: " + c.getResult());
            failAll(new Failure(expired(c.getResult()) ? "Your Steam session has ended. Sign in again." : "Steam ended the session (" + c.getResult() + ").",
                    expired(c.getResult())));
        }));
        subscriptions.add(manager.subscribe(LicenseListCallback.class, c -> {
            // Steam sends the list again after a new license, so a finished future is replaced
            CompletableFuture<List<License>> f = licenses.isDone() ? new CompletableFuture<>() : licenses;
            if (c.getResult() == EResult.OK) f.complete(c.getLicenseList());
            else f.completeExceptionally(new Failure("Steam did not send the list of your games.", false));
            licenses = f;
        }));
        subscriptions.add(manager.subscribe(PersonaStateCallback.class, c -> {
            CompletableFuture<PersonaStateCallback> f = profile;
            if (f != null && c.getFriendId().equals(me)) f.complete(c);
        }));

        Thread pump = new Thread(() -> {
            while (running) manager.runWaitCallbacks(1000L);
        }, "steam-callbacks");
        pump.setDaemon(true);
        pump.start();
    }

    void connect() throws Failure {
        connected = new CompletableFuture<>();
        client.connect();
        await(connected, 30, "Steam does not answer. Check the connection.");
    }

    /** The password goes to Steam encrypted by the library. Steam Guard goes through the prompt. */
    SteamStore.Session login(String account, String password, String device, final Prompt prompt) throws Failure {
        AuthSessionDetails details = new AuthSessionDetails();
        details.username = account;
        details.password = password;
        details.deviceFriendlyName = device;
        details.persistentSession = true;
        details.authenticator = new IAuthenticator() {
            @Override
            public CompletableFuture<String> getDeviceCode(boolean previousCodeWasIncorrect) {
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(false, null, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<String> getEmailCode(String email, boolean previousCodeWasIncorrect) {
                emailCode = true;
                CompletableFuture<String> answer = new CompletableFuture<>();
                prompt.code(true, email, previousCodeWasIncorrect, answer);
                return answer;
            }

            @Override
            public CompletableFuture<Boolean> acceptDeviceConfirmation() {
                prompt.approve();
                // Poll until the Steam app approves; a code typed meanwhile goes through sendCode
                return CompletableFuture.completedFuture(Boolean.TRUE);
            }
        };
        try {
            CredentialsAuthSession auth = client.getAuthentication().beginAuthSessionViaCredentials(details).get();
            credentials = auth;
            return finish(auth.pollingWaitForResult());
        } catch (AuthenticationException | InterruptedException | ExecutionException | CancellationException e) {
            throw explain(e);
        } finally {
            credentials = null;
        }
    }

    /** Approves a waiting password sign-in with a Steam Guard code instead of the Steam app. */
    void sendCode(String code) throws Failure {
        CredentialsAuthSession auth = credentials;
        if (auth == null) throw new Failure("The sign-in is no longer waiting for a code.", false);
        EAuthSessionGuardType type = emailCode ? EAuthSessionGuardType.k_EAuthSessionGuardType_EmailCode
                : EAuthSessionGuardType.k_EAuthSessionGuardType_DeviceCode;
        try {
            auth.sendSteamGuardCode(code, type).get(30, TimeUnit.SECONDS);
        } catch (AuthenticationException | InterruptedException | ExecutionException | TimeoutException | CancellationException e) {
            throw explain(e);
        }
    }

    /** Sign-in by scanning a QR code with the Steam app on another device. */
    SteamStore.Session loginQr(String device, final Prompt prompt) throws Failure {
        AuthSessionDetails details = new AuthSessionDetails();
        details.deviceFriendlyName = device;
        details.persistentSession = true;
        try {
            QrAuthSession auth = client.getAuthentication().beginAuthSessionViaQR(details).get();
            auth.setChallengeUrlChanged(s -> {
                if (s != null) prompt.qr(s.getChallengeUrl());
            });
            prompt.qr(auth.getChallengeUrl());
            return finish(auth.pollingWaitForResult());
        } catch (AuthenticationException | InterruptedException | ExecutionException | CancellationException e) {
            throw explain(e);
        }
    }

    private SteamStore.Session finish(CompletableFuture<AuthPollResult> poll) throws Failure {
        pending = poll;
        AuthPollResult result;
        try {
            result = poll.get();
        } catch (InterruptedException | ExecutionException | CancellationException e) {
            throw explain(e);
        } finally {
            pending = null;
        }
        resume(new SteamStore.Session(result.getAccountName(), result.getRefreshToken()));
        return new SteamStore.Session(result.getAccountName(), result.getRefreshToken());
    }

    /** Signs in with the kept session: no password, no code. */
    void resume(SteamStore.Session session) throws Failure {
        loggedOn = new CompletableFuture<>();
        licenses = new CompletableFuture<>();
        depotAccess = null;
        LogOnDetails details = new LogOnDetails();
        details.setUsername(session.account);
        details.setAccessToken(session.refreshToken);
        details.setShouldRememberPassword(true);
        // A login ID per connection, so the library screen and a download can be signed in at the same time
        details.setLoginID(new SecureRandom().nextInt(Integer.MAX_VALUE - 1) + 1);
        user.logOn(details);
        LoggedOnCallback r = await(loggedOn, 30, "Steam did not answer the sign-in.");
        if (r.getResult() != EResult.OK) {
            throw new Failure(expired(r.getResult()) ? "Your Steam session has ended. Sign in again." : "Steam refused the sign-in (" + r.getResult() + ").",
                    expired(r.getResult()));
        }
    }

    /** Which of the apps the account owns. */
    Set<Integer> owned(Set<Integer> wanted) throws Failure {
        Set<Integer> ret = new HashSet<>(wanted);
        ret.retainAll(depotAccess());
        return ret;
    }

    private Set<Integer> depotAccess;

    /** Depots the account's licenses cover: the same check DepotDownloader makes, so owned DLC counts and other DLC does not. */
    private Set<Integer> depotAccess() throws Failure {
        if (depotAccess != null) return depotAccess;
        Set<Integer> ret = new HashSet<>();
        for (PICSProductInfo info : licensedPackages()) {
            for (KeyValue id : info.getKeyValues().get("appids").getChildren()) ret.add(id.asInteger(0));
            for (KeyValue id : info.getKeyValues().get("depotids").getChildren()) ret.add(id.asInteger(0));
        }
        depotAccess = ret;
        return ret;
    }

    /** Product info of every package the account has a license for: licenses are packages, and packages list apps and depots. */
    private List<PICSProductInfo> licensedPackages() throws Failure {
        List<PICSRequest> requests = new ArrayList<>();
        for (License l : await(licenses, 30, "Steam did not send the list of your games.")) {
            requests.add(new PICSRequest(l.getPackageID(), l.getAccessToken()));
        }
        List<PICSProductInfo> ret = new ArrayList<>();
        if (requests.isEmpty()) return ret;
        AsyncJobMultiple<PICSProductInfoCallback> job = apps.picsGetProductInfo(Collections.<PICSRequest>emptyList(), requests);
        // The default 10 s is short for a large library
        job.setTimeout(60_000L);
        for (PICSProductInfoCallback c : await(job.toFuture(), 60, "Steam did not send the list of your games.").getResults()) {
            ret.addAll(c.getPackages().values());
        }
        return ret;
    }

    /** Caches which launcher games the account owns and their download languages. */
    void readLibrary(SteamStore store) throws Failure {
        Set<Integer> ids = new HashSet<>();
        for (Library.Game g : Library.GAMES) ids.add(g.appId);
        Set<Integer> mine = owned(ids);
        for (Library.Game g : Library.GAMES) {
            Map<String, Long> sizes = languages(g.appId);
            Log.i(TAG, "library: " + g.folder + " " + sizes);
            store.saveLanguages(g.appId, sizes);
        }
        store.saveOwned(mine);
        PersonaStateCallback self = profile();
        if (self != null) store.saveProfile(self.getName(), avatar(self.getAvatarHash()));
    }

    /** The account's public profile: display name and avatar. Null when Steam does not answer. */
    private PersonaStateCallback profile() {
        me = client.getSteamID();
        if (me == null) return null;
        CompletableFuture<PersonaStateCallback> f = new CompletableFuture<>();
        profile = f;
        friends.requestFriendInfo(me, 0);
        try {
            return f.get(15, TimeUnit.SECONDS);
        } catch (InterruptedException | ExecutionException | TimeoutException | CancellationException e) {
            return null;
        } finally {
            profile = null;
        }
    }

    /** The avatar on Steam's image server, null for the default one. */
    private static String avatar(byte[] hash) {
        StringBuilder hex = new StringBuilder();
        boolean set = false;
        for (byte b : hash) {
            hex.append(String.format(Locale.ROOT, "%02x", b & 0xFF));
            set |= b != 0;
        }
        return set ? "https://avatars.steamstatic.com/" + hex + "_full.jpg" : null;
    }

    /** Adds a free game to the account, the same as "Add to library" on the store page. */
    void claimFree(int appId) throws Failure {
        licenses = new CompletableFuture<>();
        depotAccess = null;
        FreeLicenseCallback r = await(apps.requestFreeLicense(appId).toFuture(), 30, "Steam did not answer the license request.");
        if (r.getResult() != EResult.OK) throw new Failure("Steam did not add the game to your library (" + r.getResult() + ").", false);
        await(licenses, 30, "Steam did not send the updated list of your games.");
    }

    /**
     * Download size per language of the Windows version: shared depots plus the language's own.
     * The key is "" when the game has no language depots.
     */
    Map<String, Long> languages(int appId) throws Failure {
        long shared = 0;
        Map<String, Long> own = new LinkedHashMap<>();
        for (Depot d : depots(appId)) {
            if (!downloads(d)) continue;
            if (d.language.isEmpty()) {
                shared += d.size;
            } else {
                Long previous = own.get(d.language);
                own.put(d.language, (previous == null ? 0 : previous) + d.size);
            }
        }
        Map<String, Long> ret = new LinkedHashMap<>();
        if (own.isEmpty()) ret.put("", shared);
        // Without a depot of its own, English is the shared files (Gothic has depots only for its other languages)
        else if (!own.containsKey("english")) ret.put("english", shared);
        for (Map.Entry<String, Long> e : own.entrySet()) ret.put(e.getKey(), shared + e.getValue());
        return ret;
    }

    /** DLC only downloads when the account owns it or the app is free to download, as DepotDownloader decides. */
    private boolean downloads(Depot d) throws Failure {
        return !d.dlc || d.free || depotAccess().contains(d.id);
    }

    private static final class Depot {
        final int id;
        final String language;
        final long size;
        final boolean dlc;
        /** The whole app downloads without a license, Archolos for example. */
        final boolean free;

        Depot(int id, String language, long size, boolean dlc, boolean free) {
            this.id = id;
            this.language = language;
            this.size = size;
            this.dlc = dlc;
            this.free = free;
        }
    }

    /** Windows depots of the app as DepotDownloader would consider them. */
    private List<Depot> depots(int appId) throws Failure {
        List<Depot> ret = new ArrayList<>();
        PICSProductInfo info = appInfo(appId);
        if (info == null) return ret;
        boolean free = info.getKeyValues().get("common").get("FreeToDownload").asBoolean(false);
        for (KeyValue depot : info.getKeyValues().get("depots").getChildren()) {
            if (!depot.getName().matches("[0-9]+")) continue;
            KeyValue config = depot.get("config");
            String os = config.get("oslist").getValue();
            String language = config.get("language").getValue();
            if (os != null && !os.isEmpty() && !os.contains("windows")) continue;
            if ("1".equals(config.get("lowviolence").getValue())) continue;
            // Redistributables from other apps never reach the phone
            if (depot.get("depotfromapp").getValue() != null) continue;
            ret.add(new Depot(Integer.parseInt(depot.getName()), language == null ? "" : language,
                    depot.get("manifests").get("public").get("size").asLong(0), depot.get("dlcappid").getValue() != null, free));
        }
        return ret;
    }

    private PICSProductInfo appInfo(int appId) throws Failure {
        Long token = await(apps.picsGetAccessTokens(appId).toFuture(), 30, "Steam did not send the game details.").getAppTokens().get(appId);
        PICSRequest request = new PICSRequest(appId, token != null ? token : 0L);
        for (PICSProductInfoCallback c : await(apps.picsGetProductInfo(request).toFuture(), 30, "Steam did not send the game details.").getResults()) {
            PICSProductInfo info = c.getApps().get(appId);
            if (info != null) return info;
        }
        return null;
    }

    /**
     * Downloads the Windows version in the chosen language into the folder. An interrupted download continues
     * from the files already there.
     */
    void download(int appId, File dir, String language, final Progress progress) throws Failure {
        List<License> list = await(licenses, 30, "Steam did not send the list of your games.");
        final boolean resume = dir.exists();
        final CompletableFuture<Void> done = new CompletableFuture<>();
        final Failure[] failed = {null};

        // DepotDownloader reports each depot's fraction; Steam's manifest sizes turn them into bytes of the whole game.
        // The total holds the depots that will download up front; a depot the app info did not list joins once Steam sends it.
        final Map<Integer, Long> depotSizes = new HashMap<>();
        final Set<Integer> counted = new HashSet<>();
        for (Depot d : depots(appId)) {
            depotSizes.put(d.id, d.size);
            if (downloads(d) && (d.language.isEmpty() || d.language.equals(language))) counted.add(d.id);
        }
        final Map<Integer, Float> parts = new HashMap<>();
        final Object lock = new Object();

        IDownloadListener listener = new IDownloadListener() {
            @Override
            public void onItemAdded(DownloadItem item) {
            }

            @Override
            public void onDownloadStarted(DownloadItem item) {
                progress.status("Downloading");
            }

            @Override
            public void onDownloadCompleted(DownloadItem item) {
                done.complete(null);
            }

            @Override
            public void onDownloadFailed(DownloadItem item, Throwable error) {
                failed[0] = new Failure("The download failed: " + error.getMessage(), false);
                done.complete(null);
            }

            @Override
            public void onStatusUpdate(String message) {
                progress.status(message);
            }

            @Override
            public void onFileCompleted(int depotId, String fileName, float depotPercentComplete) {
                part(depotId, depotPercentComplete, 0);
            }

            @Override
            public void onChunkCompleted(int depotId, float depotPercentComplete, long compressedBytes, long uncompressedBytes) {
                part(depotId, depotPercentComplete, uncompressedBytes);
            }

            @Override
            public void onDepotCompleted(int depotId, long compressedBytes, long uncompressedBytes) {
                part(depotId, 1.f, uncompressedBytes);
            }

            private void part(int depotId, float fraction, long depotBytes) {
                long got = 0;
                long total = 0;
                synchronized (lock) {
                    parts.put(depotId, Math.min(1.f, fraction));
                    counted.add(depotId);
                    // A depot Steam's app info did not list: its size follows from the bytes so far
                    if (!depotSizes.containsKey(depotId) && fraction > 0.01f && depotBytes > 0) depotSizes.put(depotId, (long) (depotBytes / fraction));
                    for (int id : counted) {
                        Long size = depotSizes.get(id);
                        if (size == null) continue;
                        total += size;
                        Float f = parts.get(id);
                        if (f != null) got += (long) (f * size);
                    }
                }
                progress.bytes(got, total);
            }
        };

        // Every step keeps a fixed buffer of chunks (about 1 MB each, twice while unpacking).
        // When writing to storage is the slowest step those buffers stay full, so a few downloads and enough writers keep memory well inside the heap
        int megabytes = (int) (Runtime.getRuntime().maxMemory() / (1024 * 1024));
        int chunks = Math.max(4, Math.min(12, megabytes / 64));
        int cores = Runtime.getRuntime().availableProcessors();
        DepotDownloader d = new DepotDownloader(client, list, false, false, chunks, Math.max(2, Math.min(4, cores)), 4, true, null);
        downloader = d;
        try {
            d.addListener(listener);
            // Windows files for every architecture in the chosen language; depots without a language come along
            d.add(new AppItem(appId, false, dir.getAbsolutePath(), null, null, false, "windows", true, null,
                    false, language.isEmpty() ? null : language, false,
                    Collections.<Integer>emptyList(), Collections.<Long>emptyList(), resume, false));
            d.finishAdding();
            while (!done.isDone() && running) {
                try {
                    done.get(1, TimeUnit.SECONDS);
                } catch (TimeoutException ignored) {
                    // Still downloading
                } catch (ExecutionException | InterruptedException e) {
                    break;
                }
            }
            if (!running && !done.isDone()) throw new Failure("Paused.", false);
            if (failed[0] != null) throw failed[0];
        } finally {
            downloader = null;
            try {
                d.close();
            } catch (Exception ignored) {
                // Already stopped
            }
        }
    }

    /** Stops whatever is running; downloaded files stay for the next attempt. */
    void cancel() {
        running = false;
        Future<?> p = pending;
        if (p != null) p.cancel(true);
        DepotDownloader d = downloader;
        if (d != null) {
            try {
                d.close();
            } catch (Exception ignored) {
                // Already stopped
            }
        }
        failAll(new Failure("Stopped.", false));
    }

    @Override
    public void close() {
        closing = true;
        cancel();
        try {
            if (client.isConnected()) user.logOff();
            client.disconnect();
        } catch (Exception ignored) {
            // Already disconnected
        }
        for (Closeable c : subscriptions) {
            try {
                c.close();
            } catch (IOException ignored) {
                // Nothing to release
            }
        }
        subscriptions.clear();
    }

    private static boolean expired(EResult r) {
        return r == EResult.AccessDenied || r == EResult.InvalidPassword || r == EResult.Expired
                || r == EResult.InvalidLoginAuthCode || r == EResult.AccountDisabled;
    }

    private void failAll(Failure f) {
        connected.completeExceptionally(f);
        loggedOn.completeExceptionally(f);
        licenses.completeExceptionally(f);
    }

    private <T> T await(CompletableFuture<T> future, int seconds, String timeout) throws Failure {
        try {
            return future.get(seconds, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            throw new Failure(timeout, false);
        } catch (InterruptedException | ExecutionException | CancellationException e) {
            throw explain(e);
        }
    }

    private static Failure explain(Throwable e) {
        if (e instanceof InterruptedException) Thread.currentThread().interrupt();
        if (e instanceof CancellationException || e instanceof InterruptedException) return new Failure("Stopped.", false);
        if (e instanceof ExecutionException && e.getCause() != null) e = e.getCause();
        if (e instanceof Failure) return (Failure) e;
        if (e instanceof AuthenticationException) {
            EResult r = ((AuthenticationException) e).getResult();
            if (r == EResult.InvalidPassword) return new Failure("Wrong account name or password.", false);
            if (r == EResult.RateLimitExceeded || r == EResult.AccountLoginDeniedThrottle) return new Failure("Too many attempts. Wait a while and try again.", false);
            if (r == EResult.TwoFactorCodeMismatch || r == EResult.InvalidLoginAuthCode) return new Failure("That code is not right.", false);
            if (r == EResult.Expired || r == EResult.FileNotFound) return new Failure("The sign-in request expired. Try again.", false);
            return new Failure("Steam refused the sign-in" + (r != null ? " (" + r + ")" : "") + ".", false);
        }
        String m = e.getMessage();
        return new Failure(m != null && !m.isEmpty() ? m : e.getClass().getSimpleName(), false);
    }
}
