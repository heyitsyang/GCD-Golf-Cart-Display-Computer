#
# Patches lv_obj.c `update_obj_state()` to soft-fail instead of writing to a
# NULL pointer when the style-transition scratch allocation fails (OOM).
#
# Root cause (crash observed on v1.0.2+43, Canned Messages screen):
#   update_obj_state() runs on EVERY widget state change - i.e. every touch
#   press and release. It unconditionally allocates a 640-byte scratch array
#   (sizeof(lv_obj_style_transition_dsc_t) * STYLE_TRANSITION_MAX == 20 * 32)
#   and then writes `ts[tsi].time = ...` WITHOUT checking the return value.
#   This is the only lv_malloc in lv_obj.c with neither a NULL check nor an
#   LV_ASSERT_MALLOC - an upstream oversight (LVGL 9.3.0, lv_obj.c:927).
#
#   When the heap is exhausted by in-flight lv_draw_task allocations (see
#   fix_lv_draw_oom.py) the malloc returns NULL and the write lands on
#   address 0x00000000:
#
#     [HEAP FAIL] size=640 caps=0x1800 in heap_caps_malloc | free=2340 largest=628
#     Guru Meditation Error: Core 0 panic'ed (StoreProhibited)
#     EXCVADDR: 0x00000000
#     ... update_obj_state <- lv_obj_remove_state <- lv_obj_event
#         <- indev_proc_release <- lv_indev_read <- lv_timer_handler <- guiTask
#
# The patch: pre-check heap_caps_get_largest_free_block before lv_malloc_zeroed
# (avoids the noisy heapAllocFailedCallback) and gate the transition-collection
# loop on `ts != NULL`. On OOM, tsi stays 0, the transition-start loop below is
# skipped, lv_free(NULL) is a no-op, and the trailing cmp_res handling still
# runs. Net effect: the object still changes state, still invalidates and still
# refreshes its style - only the 80 ms animated fade is skipped for that one
# press. No reboot.
#
# Capability flag: MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL (0x1800) is what
# plain malloc() - and therefore lv_malloc, since lv_conf.h sets
# LV_USE_STDLIB_MALLOC = LV_STDLIB_CLIB - requests on a no-PSRAM ESP32.
#
# Idempotent: sentinel check skips if already applied.
#

Import("env")
import os


SENTINEL = "GCD-OOM-OBJSTATE-PATCH"


def patch_lv_obj_state_oom():
    file_path = os.path.join(
        env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"),
        "lvgl", "src", "core", "lv_obj.c"
    )

    if not os.path.exists(file_path):
        print(f"fix_lv_obj_state_oom: {file_path} not found, skipping")
        return

    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    if SENTINEL in content:
        print("fix_lv_obj_state_oom: already applied, skipping")
        return

    # Part 1: add esp_heap_caps.h include
    OLD_INC = '#include "lv_obj_draw_private.h"'
    NEW_INC = ('#include "lv_obj_draw_private.h"\n'
               '#include "esp_heap_caps.h" /*' + SENTINEL + '*/')

    if OLD_INC not in content:
        print("fix_lv_obj_state_oom: WARNING - include anchor not found; "
              "LVGL version may have changed")
        return

    # Part 2: guarded alloc + NULL-gated collection loop
    OLD = (
        "    lv_obj_style_transition_dsc_t * ts = lv_malloc_zeroed(sizeof(lv_obj_style_transition_dsc_t) * STYLE_TRANSITION_MAX);\n"
        "    uint32_t tsi = 0;\n"
        "    uint32_t i;\n"
        "    for(i = 0; i < obj->style_cnt && tsi < STYLE_TRANSITION_MAX; i++) {"
    )
    NEW = (
        "    /*" + SENTINEL + ": pre-check heap; NULL here used to crash at ts[tsi].time*/\n"
        "    size_t _gcd_ts_size = sizeof(lv_obj_style_transition_dsc_t) * STYLE_TRANSITION_MAX;\n"
        "    lv_obj_style_transition_dsc_t * ts =\n"
        "        (heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL) >= _gcd_ts_size)\n"
        "        ? lv_malloc_zeroed(_gcd_ts_size) : NULL;\n"
        "    uint32_t tsi = 0;\n"
        "    uint32_t i;\n"
        "    for(i = 0; ts != NULL && i < obj->style_cnt && tsi < STYLE_TRANSITION_MAX; i++) {"
    )

    if OLD not in content:
        print("fix_lv_obj_state_oom: WARNING - expected text not found; "
              "LVGL version may have changed")
        return

    content = content.replace(OLD_INC, NEW_INC, 1)
    content = content.replace(OLD, NEW, 1)

    with open(file_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"fix_lv_obj_state_oom: patch applied to {file_path}")


patch_lv_obj_state_oom()
