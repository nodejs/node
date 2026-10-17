#!/usr/bin/env python3
# Downloads perfetto's trace_processor_shell, matching the SDK version vendored
# in deps/perfetto. The trace events tests read back the binary traces that
# perfetto builds write through this tool.

import os
import platform
import shutil
import stat
import sys
import tempfile
import urllib.request
import zipfile

script_dir = os.path.dirname(os.path.abspath(__file__))
version_file = os.path.join(script_dir, '..', '..', 'deps', 'perfetto', 'VERSION')
is_windows = platform.system() == 'Windows'

# Perfetto names its release archives <os>-<arch>, which does not match what
# platform reports.
OS_NAMES = {'Darwin': 'mac', 'Linux': 'linux', 'Windows': 'windows'}
ARCH_NAMES = {'x86_64': 'amd64', 'amd64': 'amd64', 'arm64': 'arm64', 'aarch64': 'arm64'}


def release_name():
  os_name = OS_NAMES.get(platform.system())
  arch = ARCH_NAMES.get(platform.machine().lower())
  if os_name is None or arch is None:
    sys.exit(f'No perfetto release for {platform.system()} {platform.machine()}')
  # Perfetto releases only an x64 build for Windows. Windows on arm64 runs it
  # under emulation.
  if is_windows:
    arch = 'amd64'
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
  with tempfile.TemporaryFile() as archive:
    with urllib.request.urlopen(url) as response:
      shutil.copyfileobj(response, archive)
    with zipfile.ZipFile(archive) as zf, \
         zf.open(f'{name}/{exe}') as src, \
         open(trace_processor, 'wb') as dst:
      shutil.copyfileobj(src, dst)

  if not is_windows:
    mode = os.stat(trace_processor).st_mode
    os.chmod(trace_processor, mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


if __name__ == '__main__':
  main()
