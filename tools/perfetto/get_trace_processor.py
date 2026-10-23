#!/usr/bin/env python3
# Downloads perfetto's trace_processor_shell, matching the SDK version vendored
# in deps/perfetto. The trace events tests read back the binary traces that
# perfetto builds write through this tool.

import io
import os
import platform
import sys
import urllib.request
import zipfile

script_dir = os.path.dirname(os.path.abspath(__file__))
version_file = os.path.join(script_dir, '..', '..', 'deps', 'perfetto', 'VERSION')
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
    sys.exit(f'No perfetto release for {platform.system()} {platform.machine()}')
  return f'{os_name}-{arch}'


def main():
  exe = 'trace_processor_shell.exe' if is_windows else 'trace_processor_shell'
  trace_processor = sys.argv[1] if len(sys.argv) > 1 else os.path.join(script_dir, exe)

  # The tool is current when it is newer than the version file it was
  # downloaded for, so a version bump re-downloads it.
  if (os.path.exists(trace_processor) and
      os.path.getmtime(trace_processor) > os.path.getmtime(version_file)):
    return

  with open(version_file) as f:
    version = f.read().strip()
  name = release_name()
  url = f'https://github.com/google/perfetto/releases/download/v{version}/{name}.zip'

  print(f'Downloading {url}')
  with urllib.request.urlopen(url) as response:
    archive = zipfile.ZipFile(io.BytesIO(response.read()))
  print(f'Extracting {name}/{exe} to {trace_processor}')
  with open(trace_processor, 'wb') as dst:
    dst.write(archive.read(f'{name}/{exe}'))
  os.chmod(trace_processor, 0o755)


if __name__ == '__main__':
  main()
