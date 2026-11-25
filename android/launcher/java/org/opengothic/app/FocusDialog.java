package org.opengothic.app;

import android.content.Context;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.appcompat.app.AlertDialog;

/** An alert dialog whose focusable views show the launcher's focus ring, so it can be used with a gamepad. */
final class FocusDialog extends AlertDialog.Builder {
    FocusDialog(Context context) {
        super(context);
    }

    @Override
    public AlertDialog create() {
        final AlertDialog dialog = super.create();
        if (dialog.getWindow() == null) return dialog;
        // The content exists only once the dialog lays out; rows a dialog rebuilds later add their rings themselves
        final View decor = dialog.getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                decor.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                LauncherActivity.addFocusRings(decor, true);
            }
        });
        return dialog;
    }
}
