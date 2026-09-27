package com.quranvr.mushaf;

import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;

/** Immersive OpenXR activity. The native render loop runs on its own thread
 *  and calls back into {@link Bridge} for UI panels and Mushaf pages. */
public class MainActivity extends Activity {
    static {
        System.loadLibrary("openxr_loader");
        System.loadLibrary("quranvr");
    }

    private native void nativeRun(Bridge bridge);
    private static native void nativeRequestExit();

    private Thread renderThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        final Bridge bridge = new Bridge(this);
        renderThread = new Thread(new Runnable() {
            @Override
            public void run() {
                nativeRun(bridge);
                bridge.shutdown();
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() { finish(); }
                });
            }
        }, "QuranVR-render");
        renderThread.start();
    }

    @Override
    protected void onDestroy() {
        nativeRequestExit();
        try {
            if (renderThread != null) renderThread.join(2000);
        } catch (InterruptedException ignored) {
        }
        super.onDestroy();
    }
}
