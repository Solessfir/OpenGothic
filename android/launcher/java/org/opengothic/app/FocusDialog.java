package org.opengothic.app;

import android.content.Context;
import android.view.View;

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
        // The content exists only once the dialog lays out, and some dialogs rebuild their rows later
        final View decor = dialog.getWindow().getDecorView();
        decor.getViewTreeObserver().addOnGlobalLayoutListener(() -> LauncherActivity.addFocusRings(decor, true));
        return dialog;
    }
}
