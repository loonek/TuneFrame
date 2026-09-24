Import("env")
import os
import shutil

def copy_factory(source, target, env):
    src = os.path.join(env.subst("$BUILD_DIR"), "firmware.factory.bin")
    dst = os.path.join(env.subst("$PROJECT_DIR"), "docs", "firmware.factory.bin")
    if os.path.exists(src):
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        print("Copied firmware.factory.bin -> docs/")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_factory)
