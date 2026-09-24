Import("env")
import os
import shutil

# After the build, copy the merged factory binary into docs/ so the web flasher
# and GitHub releases always carry the latest firmware without manual copying.
def copy_factory(source, target, env):
    src = os.path.join(env.subst("$BUILD_DIR"), "firmware.factory.bin")
    dst = os.path.join(env.subst("$PROJECT_DIR"), "docs", "firmware.factory.bin")
    if os.path.exists(src):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        print("Copied firmware.factory.bin -> docs/")

# The platform creates firmware.factory.bin in a post-action on the app bin;
# ours registers after it, so it runs once the factory bin exists.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_factory)
