import platform
import sys
import os
import subprocess

# TODO: Future versions will use a JSON file listing all patches for iteration and dynamic patch application.
def apply_patch(file_path, patch_path):
    """Apply a patch to a file using the `patch` utility."""
    try:
        if not os.path.exists(file_path) or not os.path.exists(patch_path):
            print(f"\033[91mError:\033[0m Missing file or patch: {file_path} or {patch_path}")
            return
        subprocess.run(['patch', '-f', file_path, '<', patch_path], check=True, shell=True)
        print("\033[92mInfo:\033[0m Successfully applied patch to", file_path)
    except subprocess.CalledProcessError as e:
        print(f"\033[91mError:\033[0m Failed to apply patch to {file_path}. {e}")

def patch_android():
    """Apply Android-specific patches."""
    print("- Patches List -")
    patch_list = [
        {
            "file": "./deps/v8/src/trap-handler/trap-handler.h",
            "patch": "./android-patches/trap-handler.h.patch",
            "issue": "https://github.com/nodejs/node/issues/36287"
        }
    ]
    for patch in patch_list:
        print(f"[{patch['file']}] related to {patch['issue']}")
        apply_patch(patch['file'], patch['patch'])

def validate_architecture(arch):
    """Validate the target architecture and return configuration."""
    arch_map = {
        "arm": ("arm", "armv7a-linux-androideabi"),
        "arm64": ("arm64", "aarch64-linux-android"),
        "aarch64": ("arm64", "aarch64-linux-android"),
        "x86": ("ia32", "i686-linux-android"),
        "x86_64": ("x64", "x86_64-linux-android")
    }
    if arch not in arch_map:
        print("\033[91mError:\033[0m Invalid target architecture, must be one of: arm, arm64, aarch64, x86, x86_64")
        sys.exit(1)
    return arch_map[arch]

def configure_android(android_ndk_path, android_sdk_version, arch):
    """Configure the Android build environment."""
    DEST_CPU, TOOLCHAIN_PREFIX = validate_architecture(arch)
    print("\033[92mInfo:\033[0m Configuring for", DEST_CPU, "...")

    if platform.system() == "Darwin":
        host_os = "darwin"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/darwin-x86_64")
    elif platform.system() == "Linux":
        host_os = "linux"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/linux-x86_64")
    else:
        print("\033[91mError:\033[0m Unsupported platform.")
        sys.exit(1)

    os.environ['PATH'] += os.pathsep + os.path.join(toolchain_path, "bin")
    os.environ['CC'] = f"{toolchain_path}/bin/{TOOLCHAIN_PREFIX}{android_sdk_version}-clang"
    os.environ['CXX'] = f"{toolchain_path}/bin/{TOOLCHAIN_PREFIX}{android_sdk_version}-clang++"

    GYP_DEFINES = (
        f"target_arch={arch} "
        f"v8_target_arch={arch} "
        f"android_target_arch={arch} "
        f"host_os={host_os} OS=android "
        f"android_ndk_path={android_ndk_path}"
    )
    os.environ['GYP_DEFINES'] = GYP_DEFINES

    if os.path.exists("./configure"):
        try:
            subprocess.run(
                ["./configure", f"--dest-cpu={DEST_CPU}", "--dest-os=android", "--openssl-no-asm", "--cross-compiling"],
                check=True
            )
            print("\033[92mInfo:\033[0m Configuration complete.")
        except subprocess.CalledProcessError as e:
            print("\033[91mError:\033[0m Configuration failed.", e)

def main():
    if platform.system() not in ["Linux", "Darwin"]:
        print("android-configure is currently only supported on Linux and Darwin.")
        sys.exit(1)

    if len(sys.argv) == 2 and sys.argv[1] == "patch":
        patch_android()
        sys.exit(0)

    if len(sys.argv) != 4:
        print("Usage: ./android-configure [patch] <path to the Android NDK> <Android SDK version> <target architecture>")
        sys.exit(1)

    android_ndk_path = sys.argv[1]
    android_sdk_version = sys.argv[2]
    arch = sys.argv[3]

    if not os.path.exists(android_ndk_path) or not os.listdir(android_ndk_path):
        print("\033[91mError:\033[0m Invalid path to the Android NDK")
        sys.exit(1)

    if not android_sdk_version.isdigit() or int(android_sdk_version) < 24:
        print("\033[91mError:\033[0m Android SDK version must be at least 24 (Android 7.0)")
        sys.exit(1)

    configure_android(android_ndk_path, android_sdk_version, arch)

if __name__ == "__main__":
    main()
