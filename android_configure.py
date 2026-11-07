import platform
import sys
import os
import shutil
import subprocess

# TODO: In next version, it will be a JSON file listing all the patches, and then it will iterate through to apply them.
def patch_android():
    print("- Patches List -")
    print("[1] [deps/v8/src/trap-handler/trap-handler.h] related to https://github.com/nodejs/node/issues/36287")
    if platform.system() == "Linux":
        os.system('patch -f ./deps/v8/src/trap-handler/trap-handler.h < ./android-patches/trap-handler.h.patch')
    print("\033[92mInfo: \033[0m" + "Tried to patch.")

if platform.system() != "Linux" and platform.system() != "Darwin":
    print("android-configure is currently only supported on Linux and Darwin.")
    sys.exit(1)

if len(sys.argv) == 2 and sys.argv[1] == "patch":
    patch_android()
    sys.exit(0)

if len(sys.argv) < 4:
    print("Usage: ./android-configure [patch] <path to the Android NDK> <Android SDK version> <target architecture> [configure options...]")
    sys.exit(1)

if not os.path.exists(sys.argv[1]) or not os.listdir(sys.argv[1]):
    print("\033[91mError: \033[0m" + "Invalid path to the Android NDK")
    sys.exit(1)

if int(sys.argv[2]) < 24:
    print("\033[91mError: \033[0m" + "Android SDK version must be at least 24 (Android 7.0)")
    sys.exit(1)

android_ndk_path = sys.argv[1]
android_sdk_version = sys.argv[2]
arch = sys.argv[3]
configure_options = sys.argv[4:]

if arch == "arm":
    DEST_CPU = "arm"
    TOOLCHAIN_PREFIX = "armv7a-linux-androideabi"
elif arch in ("aarch64", "arm64"):
    DEST_CPU = "arm64"
    TOOLCHAIN_PREFIX = "aarch64-linux-android"
    arch = "arm64"
elif arch == "x86":
    DEST_CPU = "ia32"
    TOOLCHAIN_PREFIX = "i686-linux-android"
elif arch == "x86_64":
    DEST_CPU = "x64"
    TOOLCHAIN_PREFIX = "x86_64-linux-android"
    arch = "x64"
else:
    print("\033[91mError: \033[0m" + "Invalid target architecture, must be one of: arm, arm64, aarch64, x86, x86_64")
    sys.exit(1)

print("\033[92mInfo: \033[0m" + "Configuring for " + DEST_CPU + "...")

if platform.system() == "Darwin":
    host_os = "darwin"
    toolchain_path = android_ndk_path + "/toolchains/llvm/prebuilt/darwin-x86_64"

elif platform.system() == "Linux":
    host_os = "linux"
    toolchain_path = android_ndk_path + "/toolchains/llvm/prebuilt/linux-x86_64"

os.environ['PATH'] += os.pathsep + toolchain_path + "/bin"
target_cc = os.path.join(toolchain_path, "bin",
                         TOOLCHAIN_PREFIX + android_sdk_version + "-clang")
target_cxx = os.path.join(toolchain_path, "bin",
                          TOOLCHAIN_PREFIX + android_sdk_version + "-clang++")

# configure.py uses CC/CXX for compiler detection, while GYP uses the
# toolset-specific variables when generating host and target rules.
os.environ['CC_target'] = (os.environ.get('CC_target') or
                           os.environ.get('CC') or target_cc)
os.environ['CXX_target'] = (os.environ.get('CXX_target') or
                            os.environ.get('CXX') or target_cxx)
os.environ['CC'] = os.environ['CC_target']
os.environ['CXX'] = os.environ['CXX_target']

host_cc_name = 'cc' if platform.system() == "Darwin" else 'gcc'
host_cxx_name = 'c++' if platform.system() == "Darwin" else 'g++'
os.environ['CC_host'] = (os.environ.get('CC_host') or
                          shutil.which(host_cc_name) or host_cc_name)
os.environ['CXX_host'] = (os.environ.get('CXX_host') or
                           shutil.which(host_cxx_name) or host_cxx_name)

GYP_DEFINES = "target_arch=" + arch
GYP_DEFINES += " v8_target_arch=" + arch
GYP_DEFINES += " android_target_arch=" + arch
GYP_DEFINES += " host_os=" + host_os + " OS=android"
GYP_DEFINES += " android_ndk_path=" + android_ndk_path
os.environ['GYP_DEFINES'] = GYP_DEFINES

if os.path.exists("./configure"):
    command = [
        "./configure",
        "--dest-cpu=" + DEST_CPU,
        "--dest-os=android",
        "--openssl-no-asm",
        "--cross-compiling",
    ] + configure_options
    subprocess.run(command, check=True)
