#
# Library updates overwrite configuration file with defaults, so this copy
# program copies customized configuration files lv_conf.h and User_Setup.h
# into their proper folders.
#
# Each file is copied only when its content differs from the destination.
# Comparing content (rather than existence) means edits to the templates
# propagate on the next build automatically, while an unchanged file is left
# untouched -- so its mtime never moves and SCons is not driven into an endless
# rebuild. That was the reason for the old exists-only guard, which silently
# discarded every template edit after the first build.
#
# Each file is also checked independently. The old guard tested only lv_conf.h
# but gated BOTH copies, so a library update that clobbered User_Setup.h would
# never be repaired as long as lv_conf.h happened to exist.
#
# Customizations to should be applied to the files in the NECESSARY TEMPLATE FILES folder
#

Import("env")

from shutil import copyfile
import os

# Optional: Add a check to ensure this runs only during a "build" action
# This prevents it from running for unrelated PlatformIO tasks like
# IntelliSense indexing or Serial Monitor initialization.
# if not env.get('PIOPLATFORM_BUILDTARGET') == 'build':
#     print(f"Not a build action - exiting")
#     Exit(0) 

print("Copying CYD configuration files...")

# Define source and destination paths
# Ensure "NECESSARY TEMPLATE FILES" is in your project root
template_dir = os.path.join(env.get("PROJECT_DIR"), "NECESSARY TEMPLATE FILES") 

# Verify the existence of the template directory
if not os.path.isdir(template_dir):
    print(f"Error: Template directory not found: {template_dir}")
    # You might want to raise an exception or exit here if it's critical
    Exit(1) # Exit with an error code

lv_conf_src = os.path.join(template_dir, "lv_conf.h")
# This destination assumes the '.pio/libdeps/cyd' folder exists and lv_conf.h goes there
lv_conf_dest = os.path.join(env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"), "lv_conf.h")

user_setup_src = os.path.join(template_dir, "User_Setup.h")
# This destination assumes the the '.pio/libdeps/cyd/TFT_eSPI' folder exists and User_Setup.h goes into its root
user_setup_dest = os.path.join(env.get("PROJECT_LIBDEPS_DIR"), env.get("PIOENV"), "TFT_eSPI", "User_Setup.h")

# Ensure destination directories exist
os.makedirs(os.path.dirname(lv_conf_dest), exist_ok=True)
os.makedirs(os.path.dirname(user_setup_dest), exist_ok=True)


def copy_if_changed(src, dest, label):
    """Copy src->dest only when the content differs, so unchanged files keep
    their mtime and do not trigger a needless LVGL/TFT_eSPI rebuild."""
    if not os.path.exists(src):
        print(f"Error: template not found: {src}")
        Exit(1)

    if os.path.exists(dest):
        with open(src, "rb") as f:
            src_bytes = f.read()
        with open(dest, "rb") as f:
            dest_bytes = f.read()
        if src_bytes == dest_bytes:
            print(f"{label}: up to date")
            return

    try:
        copyfile(src, dest)
        print(f"{label}: UPDATED -> {dest}")
    except Exception as e:
        print(f"{label}: error copying: {e}")
        Exit(1)


copy_if_changed(lv_conf_src, lv_conf_dest, "lv_conf.h")
copy_if_changed(user_setup_src, user_setup_dest, "User_Setup.h")


