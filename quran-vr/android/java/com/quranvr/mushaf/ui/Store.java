package com.quranvr.mushaf.ui;

/** Tiny persistence abstraction (SharedPreferences on the headset). */
public interface Store {
    int getInt(String key, int def);
    void putInt(String key, int value);
    String getString(String key, String def);
    void putString(String key, String value);
}
