import platform
import sys
import os

# TODO: In next version, it will be a JSON file listing all the patches, and then it will iterate through to apply them.
def patch_android():
    print("- Patches List -")
    print("[1] [deps/v8/src/trap-handler/trap-handler.h] related to https://github.com/nodejs/node/issues/36287")
    if platform.system() == "Linux":
        os.system('patch -f ./deps/v8/src/trap-handler/trap-handler.h < ./android-patches/trap-handler.h.patch')
    print("\033[92mInfo: \033[0m" + "Tried to patch.")

if platform.system() == "Windows":
    print("android-configure is not supported on Windows yet.")
    sys.exit(1)

if len(sys.argv) == 2 and sys.argv[1] == "patch":
    patch_android()
    sys.exit(0)

if len(sys.argv) != 4:
    print("Usage: ./android-configure [patch] <path to the Android NDK> <Android SDK version> <target architecture>")
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
os.environ['CC'] = toolchain_path + "/bin/" + TOOLCHAIN_PREFIX + android_sdk_version + "-" +  "clang"
os.environ['CXX'] = toolchain_path + "/bin/" + TOOLCHAIN_PREFIX + android_sdk_version + "-" + "clang++"

GYP_DEFINES = "target_arch=" + arch
GYP_DEFINES += " v8_target_arch=" + arch
GYP_DEFINES += " android_target_arch=" + arch
GYP_DEFINES += " host_os=" + host_os + " OS=android"
GYP_DEFINES += " android_ndk_path=" + android_ndk_path
os.environ['GYP_DEFINES'] = GYP_DEFINES

if os.path.exists("./configure"):
    os.system("./configure --dest-cpu=" + DEST_CPU + " --dest-os=android --openssl-no-asm --cross-compiling")


import platform
import sys
import os
import argparse

# Funciones mejoradas

def print_patches():
    print("- Lista de Parches -")
    print("[1] [deps/v8/src/trap-handler/trap-handler.h] relacionado con https://github.com/nodejs/node/issues/36287")

def apply_android_patch():
    if platform.system() == "Linux":
        os.system('patch -f ./deps/v8/src/trap-handler/trap-handler.h < ./android-patches/trap-handler.h.patch')
    print("\033[92mInfo: \033[0m" + "Intentó aplicar el parche.")

def patch_android():
    print_patches()
    apply_android_patch()

def configure_android(android_ndk_path, android_sdk_version, arch):
    # Configuración específica para Android
    print("\033[92mInfo: \033[0m" + "Configurando para " + DEST_CPU + "...")
    
    if platform.system() == "Darwin":
        host_os = "darwin"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/darwin-x86_64")
    elif platform.system() == "Linux":
        host_os = "linux"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/linux-x86_64")
    
    # Actualizar la variable de entorno 'PATH'
    os.environ['PATH'] += os.pathsep + os.path.join(toolchain_path, "bin")
    os.environ['CC'] = os.path.join(toolchain_path, "bin", TOOLCHAIN_PREFIX + android_sdk_version + "-" +  "clang")
    os.environ['CXX'] = os.path.join(toolchain_path, "bin", TOOLCHAIN_PREFIX + android_sdk_version + "-" + "clang++")

    # Definir variables de entorno para GYP
    GYP_DEFINES = f"target_arch={arch} v8_target_arch={arch} android_target_arch={arch} host_os={host_os} OS=android android_ndk_path={android_ndk_path}"
    os.environ['GYP_DEFINES'] = GYP_DEFINES

    # Verificar la existencia del archivo 'configure'
    if os.path.exists("./configure"):
        os.system("./configure --dest-cpu=" + DEST_CPU + " --dest-os=android --openssl-no-asm --cross-compiling")

# Constantes
DEST_CPU = ""
TOOLCHAIN_PREFIX = ""

def main():
    # Configuración de argumentos de línea de comandos
    parser = argparse.ArgumentParser(description="Configurar y parchear para Android.")
    parser.add_argument("ndk_path", help="Ruta al Android NDK")
    parser.add_argument("sdk_version", type=int, help="Versión de Android SDK (mínimo 24)")
    parser.add_argument("arch", choices=["arm", "arm64", "x86", "x86_64"], help="Arquitectura de destino: arm, arm64, x86, x86_64")

    args = parser.parse_args()

    # Validaciones
    if not os.path.exists(args.ndk_path) or not os.listdir(args.ndk_path):
        print("\033[91mError: \033[0m" + "Ruta inválida al Android NDK")
        sys.exit(1)

    if args.sdk_version < 24:
        print("\033[91mError: \033[0m" + "La versión de Android SDK debe ser al menos 24 (Android 7.0)")
        sys.exit(1)

    # Configuración y parcheo
    configure_android(args.ndk_path, str(args.sdk_version), args.arch)
    patch_android()

if __name__ == "__main__":
    main()
  
import platform
import sys
import os
import argparse

# Funciones mejoradas #

def print_patches():
    print("- Lista de Parches -")
    print("[1] [deps/v8/src/trap-handler/trap-handler.h] relacionado con https://github.com/nodejs/node/issues/36287")

def apply_android_patch():
    if platform.system() == "Linux":
        os.system('patch -f ./deps/v8/src/trap-handler/trap-handler.h < ./android-patches/trap-handler.h.patch')
    print("\033[92mInfo: \033[0m" + "Intentó aplicar el parche.")

def patch_android():
    print_patches()
    apply_android_patch()

def configure_android(android_ndk_path, android_sdk_version, arch):
    # Configuración específica para Android
    print("\033[92mInfo: \033[0m" + "Configurando para " + DEST_CPU + "...")
    
    if platform.system() == "Darwin":
        host_os = "darwin"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/darwin-x86_64")
    elif platform.system() == "Linux":
        host_os = "linux"
        toolchain_path = os.path.join(android_ndk_path, "toolchains/llvm/prebuilt/linux-x86_64")
    
    # Actualizar la variable de entorno 'PATH'
    os.environ['PATH'] += os.pathsep + os.path.join(toolchain_path, "bin")
    os.environ['CC'] = os.path.join(toolchain_path, "bin", TOOLCHAIN_PREFIX + android_sdk_version + "-" +  "clang")
    os.environ['CXX'] = os.path.join(toolchain_path, "bin", TOOLCHAIN_PREFIX + android_sdk_version + "-" + "clang++")

    # Definir variables de entorno para GYP
    GYP_DEFINES = f"target_arch={arch} v8_target_arch={arch} android_target_arch={arch} host_os={host_os} OS=android android_ndk_path={android_ndk_path}"
    os.environ['GYP_DEFINES'] = GYP_DEFINES

    # Verificar la existencia del archivo 'configure'
    if os.path.exists("./configure"):
        os.system("./configure --dest-cpu=" + DEST_CPU + " --dest-os=android --openssl-no-asm --cross-compiling")
