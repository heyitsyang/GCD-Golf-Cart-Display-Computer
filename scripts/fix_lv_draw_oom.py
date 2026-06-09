#
# Patches lv_draw.c to soft-fail instead of asserting when a draw task
# allocation fails (OOM).
#
# Root cause: Settings2 (and other complex screens) accumulate enough
# lv_draw_task_t+descriptor allocations across all tiles before any are freed
# that the heap is exhausted. Without this patch, LV_ASSERT_MALLOC halts the
# device (while(1)) and the watchdog fires ~12s later.
#
# The patch: pre-check heap_caps_get_largest_free_block before lv_malloc_zeroed
# (avoids heapAllocFailedCallback) and, on OOM, return a static dummy task that
# carries a valid draw_dsc pointer but is NOT linked into the layer's task list.
# The caller writes to it and calls lv_draw_finalize_task_creation, which marks
# the dummy READY (no draw unit claims type=0). The widget is silently skipped
# for that render cycle; on the next dirty-refresh it typically succeeds.
#
# Patch is idempotent: checks for sentinel before applying.
#

Import("env")
import os


def patch_lv_draw_oom():
    file_path = os.path.join(
        env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"),
        "lvgl", "src", "draw", "lv_draw.c"
    )

    if not os.path.exists(file_path):
        print(f"fix_lv_draw_oom: {file_path} not found, skipping")
        return

    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    SENTINEL = "GCD-OOM-DRAW-PATCH"
    if SENTINEL in content:
        print("fix_lv_draw_oom: already applied, skipping")
        return

    # --- Part 1: add esp_heap_caps.h include ---
    OLD_INC = '#include "../stdlib/lv_string.h"'
    NEW_INC = '#include "../stdlib/lv_string.h"\n#include "esp_heap_caps.h" /*GCD-OOM-DRAW-PATCH*/'

    if OLD_INC not in content:
        print("fix_lv_draw_oom: WARNING - include anchor not found; LVGL version may have changed")
        return

    content = content.replace(OLD_INC, NEW_INC, 1)

    # --- Part 2: replace LV_ASSERT_MALLOC with pre-check + dummy-task fallback ---
    OLD = (
        "    lv_draw_task_t * new_task = lv_malloc_zeroed(LV_ALIGN_UP(sizeof(lv_draw_task_t), 8) + dsc_size);\n"
        "    LV_ASSERT_MALLOC(new_task);\n"
        "    new_task->area = *coords;"
    )
    NEW = (
        "    size_t _gcd_alloc = LV_ALIGN_UP(sizeof(lv_draw_task_t), 8) + dsc_size; /*GCD-OOM-DRAW-PATCH*/\n"
        "    lv_draw_task_t * new_task =\n"
        "        (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) >= _gcd_alloc)\n"
        "        ? lv_malloc_zeroed(_gcd_alloc) : NULL;\n"
        "    if(new_task == NULL) { /*GCD-OOM-DRAW-PATCH: return dummy; not linked into task list*/\n"
        "        static uint8_t s_oom_draw_buf[LV_ALIGN_UP(sizeof(lv_draw_task_t), 8) + 512];\n"
        "        lv_memzero(s_oom_draw_buf, sizeof(s_oom_draw_buf));\n"
        "        lv_draw_task_t * oom_task = (lv_draw_task_t *)s_oom_draw_buf;\n"
        "        oom_task->draw_dsc = s_oom_draw_buf + LV_ALIGN_UP(sizeof(lv_draw_task_t), 8);\n"
        "        LV_PROFILER_DRAW_END;\n"
        "        return oom_task;\n"
        "    } /*GCD-OOM-DRAW-PATCH end*/\n"
        "    new_task->area = *coords;"
    )

    if OLD not in content:
        print(
            "fix_lv_draw_oom: WARNING - expected text not found; "
            "LVGL version may have changed"
        )
        return

    content = content.replace(OLD, NEW, 1)

    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"fix_lv_draw_oom: patch applied to {file_path}")


patch_lv_draw_oom()
