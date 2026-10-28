# SPDX-License-Identifier: GPL-3.0-or-later
"""PlatformIO post-build hook; uses the same validated packager as manual builds."""
import subprocess

Import("env")


def package_uf2(source, target, env):
    subprocess.run([
        env.subst("$PYTHONEXE"), env.subst("$PROJECT_DIR/tools/make_uf2.py"),
        str(target[0]), env.subst("$BUILD_DIR/firmware.uf2"),
    ], check=True)


env.AddPostAction("$BUILD_DIR/firmware.bin", package_uf2)
env.Depends("$BUILD_DIR/firmware.bin", "$PROJECT_DIR/tools/make_uf2.py")
