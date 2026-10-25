#!/usr/bin/env python3
# Downloads perfetto's trace_processor_shell, matching the SDK version vendored
# in deps/perfetto. The trace events tests read back the binary traces that
# perfetto builds write through this tool.

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


def release_name():
  # Perfetto releases only an x64 build for Windows. Windows on arm64 runs it
  # under emulation.
  if is_windows:
    return 'windows-amd64'

  os_name = OS_NAMES.get(platform.system())
  arch = ARCH_NAMES.get(platform.machine())
  if os_name is None or arch is None:
    return None
  return f'{os_name}-{arch}'


def download(url):
  print(f'Downloading {url}')
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
    print(f'Extracting {name}/{exe}')
    data = archive.read(f'{name}/{exe}')

  print(f'Writing {trace_processor}')
  with open(trace_processor, 'wb') as dst:
    dst.write(data)
  os.chmod(trace_processor, 0o755)


if __name__ == '__main__':
  main()
