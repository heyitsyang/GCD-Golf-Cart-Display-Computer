#
# Patches lv_draw_label.c to soft-fail instead of aborting when the glyph
# draw buffer allocation fails (OOM).
#
# Root cause: ui_font_hot_logo_172 (172px) renders an A8 glyph at 128x192 px,
# requiring ~24.6 KB contiguous heap. With LV_STDLIB_CLIB (mandatory on GCD),
# heap fragmentation from the 14-screen ui_init() occasionally means no
# contiguous 24.6 KB block exists when the splash screen first renders.
# Without this patch, LV_ASSERT_MALLOC fires abort() -> watchdog -> reboot.
#
# The patch: replace the hard assert at lv_draw_label.c line ~609 with a
# conditional that nulls dsc->_draw_buf and returns, skipping the glyph.
# The splash "h" logo is invisible that frame, but the device boots normally.
# If the glyph_guard fix (main.cpp / gui_task.cpp) provides the 26 KB gap,
# the "h" renders fine and this patch is never triggered.
#
# Patch is idempotent: checks for sentinel before applying.
#

Import("env")
import os


def patch_lv_draw_label():
    file_path = os.path.join(
        env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"),
        "lvgl", "src", "draw", "lv_draw_label.c"
    )

    if not os.path.exists(file_path):
        print(f"fix_lv_draw_label_oom: {file_path} not found, skipping")
        return

    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    SENTINEL = "GCD-OOM-PATCH"
    if SENTINEL in content:
        print("fix_lv_draw_label_oom: already applied, skipping")
        return

    OLD = "                LV_ASSERT_MALLOC(draw_buf);"
    # lv_draw_buf_destroy was called on the old _draw_buf (line 605) before we
    # get here, so _draw_buf is a dangling pointer.  Null it before returning.
    NEW = (
        "                if(!draw_buf) { dsc->_draw_buf = NULL; "
        'LV_LOG_WARN("[' + SENTINEL + '] OOM: skipping glyph draw_buf"); '
        "return; }"
    )

    if OLD not in content:
        print(
            "fix_lv_draw_label_oom: WARNING - expected text not found; "
            "LVGL version may have changed"
        )
        return

    content = content.replace(OLD, NEW, 1)
    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"fix_lv_draw_label_oom: patch applied to {file_path}")


patch_lv_draw_label()
