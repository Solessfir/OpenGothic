package org.opengothic.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/**
 * The Steam sign-in kept between launches: the account name and a refresh token, never the password.
 * The token is encrypted with an Android Keystore key that never leaves the device; when the Keystore fails, nothing is kept.
 * Also caches what the library shows, so it appears at once and offline.
 */
final class SteamStore {
    static final class Session {
        final String account;
        final String refreshToken;

        Session(String account, String refreshToken) {
            this.account = account;
            this.refreshToken = refreshToken;
        }
    }

    private static final String ALIAS = "opengothic.steam";
    private final SharedPreferences prefs;

    SteamStore(Context context) {
        prefs = context.getSharedPreferences("steam", Context.MODE_PRIVATE);
    }

    String account() {
        return prefs.getString("token", null) == null ? null : prefs.getString("account", null);
    }

    Session load() {
        String account = prefs.getString("account", null);
        String iv = prefs.getString("iv", null);
        String token = prefs.getString("token", null);
        if (account == null || iv == null || token == null) return null;
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, Base64.decode(iv, Base64.NO_WRAP)));
            return new Session(account, new String(cipher.doFinal(Base64.decode(token, Base64.NO_WRAP)), StandardCharsets.UTF_8));
        } catch (Exception e) {
            // The key is gone, e.g. after the screen lock changed: sign in again
            clear();
            return null;
        }
    }

    /** False when it cannot be kept safely. */
    boolean save(Session s) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true));
            byte[] data = cipher.doFinal(s.refreshToken.getBytes(StandardCharsets.UTF_8));
            prefs.edit()
                    .putString("account", s.account)
                    .putString("iv", Base64.encodeToString(cipher.getIV(), Base64.NO_WRAP))
                    .putString("token", Base64.encodeToString(data, Base64.NO_WRAP))
                    .apply();
            return true;
        } catch (Exception e) {
            clear();
            return false;
        }
    }

    /** The display name from the Steam profile; the account name is the login and always lower case. */
    void saveProfile(String name, String avatar) {
        prefs.edit().putString("name", name).putString("avatar", avatar).apply();
    }

    String name() {
        String name = prefs.getString("name", null);
        return name == null || name.isEmpty() ? account() : name;
    }

    String avatar() {
        return prefs.getString("avatar", null);
    }

    void saveOwned(Set<Integer> apps) {
        StringBuilder s = new StringBuilder();
        for (int a : apps) s.append(s.length() > 0 ? "," : "").append(a);
        prefs.edit().putString("owned", s.toString()).apply();
    }

    /** Null until the library was read from Steam once. */
    Set<Integer> owned() {
        String s = prefs.getString("owned", null);
        if (s == null) return null;
        Set<Integer> ret = new HashSet<>();
        for (String part : s.split(",")) {
            try {
                ret.add(Integer.parseInt(part.trim()));
            } catch (NumberFormatException ignored) {
                // An empty library
            }
        }
        return ret;
    }

    /** Download size per Steam language; "" when the game has no language choice. */
    void saveLanguages(int appId, Map<String, Long> sizes) {
        StringBuilder s = new StringBuilder();
        for (Map.Entry<String, Long> e : sizes.entrySet()) s.append(s.length() > 0 ? "," : "").append(e.getKey()).append(':').append(e.getValue());
        prefs.edit().putString("languages_" + appId, s.toString()).apply();
    }

    Map<String, Long> languages(int appId) {
        Map<String, Long> ret = new LinkedHashMap<>();
        String s = prefs.getString("languages_" + appId, "");
        for (String part : s.split(",")) {
            int colon = part.lastIndexOf(':');
            if (colon < 0) continue;
            try {
                ret.put(part.substring(0, colon), Long.parseLong(part.substring(colon + 1)));
            } catch (NumberFormatException ignored) {
                // Written by an older version
            }
        }
        return ret;
    }

    void clear() {
        prefs.edit().clear().apply();
        try {
            KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
            ks.load(null);
            ks.deleteEntry(ALIAS);
        } catch (Exception ignored) {
            // Nothing to delete
        }
    }

    private static SecretKey key(boolean create) throws Exception {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        if (ks.containsAlias(ALIAS)) return (SecretKey) ks.getKey(ALIAS, null);
        if (!create) throw new IllegalStateException("No key");
        KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        gen.init(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setKeySize(256)
                .build());
        return gen.generateKey();
    }
}
