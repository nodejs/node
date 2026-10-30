#!/usr/bin/env python3
# Downloads perfetto's trace_processor_shell, matching the SDK version vendored
# in deps/perfetto. The trace events tests read back the binary traces that
# perfetto builds write through this tool.

import ctypes
import io
import json
import os
import platform
import sys
import urllib.request
import zipfile

script_dir = os.path.dirname(os.path.abspath(__file__))

version_file = os.path.join(script_dir, '..', '..', 'deps', 'perfetto', 'VERSION')
# Platforms perfetto publishes no release for, cross-compiled and pushed to the
# nodejs Cachix cache by tools/dep_updaters/update-nixpkgs-pin.sh.
cross_compile_info = os.path.join(script_dir, 'trace_processor_shell.json')
is_windows = platform.system() == 'Windows'

# Perfetto names its release archives <os>-<arch>, which does not match what
# platform reports.
OS_NAMES = {'Darwin': 'mac', 'Linux': 'linux'}
ARCH_NAMES = {'x86_64': 'amd64', 'arm64': 'arm64', 'aarch64': 'arm64'}


def get_glibc_version():
    """
    Returns a tuple like (2, 34) if glibc is present, or None if not.
    """
    # 1. Primary check via POSIX confstr
    try:
        val = os.confstr("CS_GNU_LIBC_VERSION")
        if val:
            # val is typically formatted like "glibc 2.31"
            _, ver = val.split()
            return tuple(map(int, ver.split(".")[:2]))
    except (AttributeError, OSError, ValueError):
        pass

    # 2. Fallback using ctypes to query the runtime linker
    try:
        libc = ctypes.CDLL(None)
        if hasattr(libc, "gnu_get_libc_version"):
            libc.gnu_get_libc_version.restype = ctypes.c_char_p
            ver = libc.gnu_get_libc_version().decode("ascii")
            return tuple(map(int, ver.split(".")[:2]))
    except Exception:
        pass

    return None

def cpu_supports_perfetto_prebuilt(arch):
    """
    Checks if the x86_64 CPU supports AVX2 and BMI2 required by
    Perfetto's release prebuilts.
    """
    if sys.platform != "linux" or arch != "amd64":
        return True

    flags = set()
    try:
        with open("/proc/cpuinfo", "r") as f:
            for line in f:
                if line.startswith("flags"):
                    flags.update(line.split(":")[1].strip().split())
                    break
    except Exception:
        return False

    # Complete feature list matching Perfetto's CheckCpuOptimizations()
    required_flags = {"sse4_2", "popcnt", "avx2", "bmi1", "bmi2"}
    missing = required_flags - flags

    # Check LZCNT (kernel flag 'abm' or synthesized 'lzcnt')
    if not ("abm" in flags or "lzcnt" in flags):
      missing.add("lzcnt (abm)")

    if missing:
      print(f"get_trace_processor: CPU is missing required Perfetto features: {sorted(missing)}")
      return False

    return True

def release_name():
  # Perfetto releases only an x64 build for Windows. Windows on arm64 runs it
  # under emulation.
  if is_windows:
    return 'windows-amd64'

  os_name = OS_NAMES.get(platform.system())
  arch = ARCH_NAMES.get(platform.machine())
  print(f'get_trace_processor: Detected {os_name}-{arch}')

  # Perfetto's Linux releases link against glibc. On musl (e.g. Alpine), use
  # the static musl build from Cachix instead.
  if os_name == 'linux':
    glibc_ver = get_glibc_version()
    if glibc_ver is None:
      print('get_trace_processor: No glibc detected, switching to musl build')
      return None
    print(f'get_trace_processor: Found glibc {glibc_ver}')
    if glibc_ver < (2, 29):
      print('get_trace_processor: Insufficient glibc version, switching to musl build')
      return None
    if not cpu_supports_perfetto_prebuilt(arch):
      print('get_trace_processor: Current platform does not support pre-built binaries, switching to musl build')
      return None

  if os_name is None or arch is None:
    return None
  return f'{os_name}-{arch}'


def download(url):
  print(f'get_trace_processor: Downloading {url}')
  # Cachix's CDN rejects urllib's default User-Agent.
  request = urllib.request.Request(url, headers={'User-Agent': 'node-get-trace-processor'})
  with urllib.request.urlopen(request) as response:
    return response.read()


def download_from_cachix():
  name = f'{platform.system().lower()}-{platform.machine()}'
  with open(cross_compile_info) as f:
    entry = json.load(f).get(name, {})
  if 'storePath' not in entry:
    sys.exit(f'No perfetto release for {platform.system()} {platform.machine()}')

  _, _, _, store_name, artifact = entry['storePath'].split('/', 4)
  store_hash = store_name.split('-', 1)[0]
  data = download('https://app.cachix.org/api/v1/cache/nodejs/serve/'
                  f'{store_hash}/{artifact}')
  return data


def main():
  exe = 'trace_processor_shell.exe' if is_windows else 'trace_processor_shell'
  trace_processor = sys.argv[1] if len(sys.argv) > 1 else os.path.join(script_dir, exe)

  # The tool is current when it is newer than the files it was downloaded
  # for, so a version bump re-downloads it.
  if (os.path.exists(trace_processor) and
      os.path.getmtime(trace_processor) > max(os.path.getmtime(version_file),
                                              os.path.getmtime(cross_compile_info))):
    return

  name = release_name()
  if name is None:
    data = download_from_cachix()
  else:
    with open(version_file) as f:
      version = f.read().strip()
    url = f'https://github.com/google/perfetto/releases/download/v{version}/{name}.zip'
    archive = zipfile.ZipFile(io.BytesIO(download(url)))
    print(f'get_trace_processor: Extracting {name}/{exe}')
    data = archive.read(f'{name}/{exe}')

  print(f'get_trace_processor: Writing {trace_processor}')
  with open(trace_processor, 'wb') as dst:
    dst.write(data)
  os.chmod(trace_processor, 0o755)


if __name__ == '__main__':
  main()
