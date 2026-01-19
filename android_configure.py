import platform
import sys
import os
import json


def patch_android():
    print("- Patches List -")

    try:
        with open('android-patches.json', 'r') as f:
            config = json.load(f)
    except FileNotFoundError:
        print("\033[91mError: \033[0m" +
              "android-patches.json not found. This file is required for Android patch management.")
        return
    except json.JSONDecodeError as e:
        print("\033[91mError: \033[0m" +
              f"Invalid JSON in android-patches.json: {e}")
        return

    patches = config.get('patches', [])
    if not patches:
        print("\033[93mWarning: \033[0m" +
              "No patches found in configuration.")
        print("\033[92mInfo: \033[0m" + "Tried to patch.")
        return

    current_platform = platform.system()
    patch_number = 1
    patches_applied = 0

    for patch in patches:
        try:
            name = patch.get('name', 'Unknown Patch')
            target_file = patch.get('target_file', '')
            patch_file = patch.get('patch_file', '')
            platforms = patch.get('platforms', [])
            description = patch.get('description', '')
            required = patch.get('required', True)

            # Display patch information
            if description:
                print(f"[{patch_number}] [{target_file}] {description}")
            else:
                print(f"[{patch_number}] [{target_file}] {name}")

            # Check if patch applies to current platform
            if platforms and current_platform not in platforms:
                print(f"    Skipping: Not applicable to {current_platform}")
                patch_number += 1
                continue

            # Check if patch file exists
            if not os.path.exists(patch_file):
                error_msg = f"Patch file not found: {patch_file}"
                if required:
                    print(f"\033[91mError: \033[0m{error_msg}")
                    return
                else:
                    print(f"\033[93mWarning: \033[0m{error_msg}")
                    patch_number += 1
                    continue

            # Check if target file exists
            if not os.path.exists(target_file):
                error_msg = f"Target file not found: {target_file}"
                if required:
                    print(f"\033[91mError: \033[0m{error_msg}")
                    return
                else:
                    print(f"\033[93mWarning: \033[0m{error_msg}")
                    patch_number += 1
                    continue

            # Apply the patch
            result = os.system(f'patch -f {target_file} < {patch_file}')
            if result == 0:
                patches_applied += 1
            elif required:
                print(
                    f"\033[91mError: \033[0mFailed to apply required patch: {name}")
                return

            patch_number += 1

        except KeyError as e:
            print(
                f"\033[91mError: \033[0mMissing required field in patch configuration: {e}")
            if patch.get('required', True):
                return
            patch_number += 1
            continue

    print("\033[92mInfo: \033[0m" + "Tried to patch.")


if platform.system() != "Linux" and platform.system() != "Darwin":
    print("android-configure is currently only supported on Linux and Darwin.")
    sys.exit(1)

if len(sys.argv) == 2 and sys.argv[1] == "patch":
    patch_android()
    sys.exit(0)

if len(sys.argv) != 4:
    print(
        "Usage: ./android-configure [patch] <path to the Android NDK> <Android SDK version> <target architecture>")
    sys.exit(1)

if not os.path.exists(sys.argv[1]) or not os.listdir(sys.argv[1]):
    print("\033[91mError: \033[0m" + "Invalid path to the Android NDK")
    sys.exit(1)

if int(sys.argv[2]) < 24:
    print("\033[91mError: \033[0m" +
          "Android SDK version must be at least 24 (Android 7.0)")
    sys.exit(1)

android_ndk_path = sys.argv[1]
android_sdk_version = sys.argv[2]
arch = sys.argv[3]

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
    print("\033[91mError: \033[0m" +
          "Invalid target architecture, must be one of: arm, arm64, aarch64, x86, x86_64")
    sys.exit(1)

print("\033[92mInfo: \033[0m" + "Configuring for " + DEST_CPU + "...")

if platform.system() == "Darwin":
    host_os = "darwin"
    toolchain_path = android_ndk_path + "/toolchains/llvm/prebuilt/darwin-x86_64"

elif platform.system() == "Linux":
    host_os = "linux"
    toolchain_path = android_ndk_path + "/toolchains/llvm/prebuilt/linux-x86_64"

os.environ['PATH'] += os.pathsep + toolchain_path + "/bin"
os.environ['CC'] = toolchain_path + "/bin/" + \
    TOOLCHAIN_PREFIX + android_sdk_version + "-" + "clang"
os.environ['CXX'] = toolchain_path + "/bin/" + \
    TOOLCHAIN_PREFIX + android_sdk_version + "-" + "clang++"

GYP_DEFINES = "target_arch=" + arch
GYP_DEFINES += " v8_target_arch=" + arch
GYP_DEFINES += " android_target_arch=" + arch
GYP_DEFINES += " host_os=" + host_os + " OS=android"
GYP_DEFINES += " android_ndk_path=" + android_ndk_path
os.environ['GYP_DEFINES'] = GYP_DEFINES

if os.path.exists("./configure"):
    os.system("./configure --dest-cpu=" + DEST_CPU +
              " --dest-os=android --openssl-no-asm --cross-compiling")
