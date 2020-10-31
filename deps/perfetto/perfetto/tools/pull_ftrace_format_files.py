#!/usr/bin/env python
# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function
import argparse
import datetime
import os
import subprocess
import sys
"""Pulls all format files from an Android device.

Usage: ./tools/pull_ftrace_format_files.py [-s serial] [-p directory_prefix]
"""

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ADB_PATH = os.path.join(ROOT_DIR, 'buildtools/android_sdk/platform-tools/adb')


def adb(*cmd, **kwargs):
  serial = kwargs.get('serial', None)
  prefix = [ADB_PATH]
  if serial:
    prefix += ['-s', serial]
  cmd = prefix + list(cmd)
  output = subprocess.check_output(cmd).replace('\r', '')
  return output


def get_devices():
  #  adb devices output looks like:
  #    List of devices attached
  #    557dccd8\tdevice
  #  With a trailing newline.
  serials = [s.split('\t')[0] for s in adb('devices').split('\n')[1:] if s]
  return serials


def ensure_output_directory_empty(path):
  if os.path.isfile(path):
    print('The output directory {} exists as a file.'.format(path))
    sys.exit(1)

  if os.path.isdir(path) and os.listdir(path):
    print('The output directory {} exists but is not empty.'.format(path))
    sys.exit(1)

  if not os.path.isdir(path):
    os.makedirs(path)


def ensure_dir(path):
  try:
    os.makedirs(path)
  except OSError:
    if not os.path.isdir(path):
      raise


def ensure_single_device(serial):
  serials = get_devices()
  if serial is None and len(serials) == 1:
    return serials[0]

  if serial in serials:
    return serial

  if not serials:
    print('No devices connected.')
  elif serial is None:
    print('More than one device connected, use -s.')
  else:
    print('No device with serial {} found.'.format(serial))
  sys.exit(1)


def pull_format_files(serial, output_directory):
  # Pulling each file individually is 100x slower so we pipe all together then
  # split them on the host.
  cmd = "find /sys/kernel/debug/tracing/ " \
      "-name available_events -o " \
      "-name format -o " \
      "-name header_event -o " \
      "-name header_page | " \
      "grep -v '/instances/' | " \
      "while read f; do echo 'path:' $f; cat $f; done"

  output = adb('shell', cmd, serial=serial)
  sections = output.split('path: /sys/kernel/debug/tracing/')
  for section in sections:
    if not section:
      continue
    path, rest = section.split('\n', 1)
    path = os.path.join(output_directory, path)
    ensure_dir(os.path.dirname(path))
    with open(path, 'wb') as f:
      f.write(rest)


# Produces output of the form: prefix_android_seed_N2F62_3.10.49
def get_output_directory(prefix=None, serial=None):
  build_id = adb('shell', 'getprop', 'ro.build.id', serial=serial).strip()
  product = adb('shell', 'getprop', 'ro.build.product', serial=serial).strip()
  kernel = adb('shell', 'uname', '-r', serial=serial).split('-')[0].strip()
  parts = ['android', product, build_id, kernel]
  if prefix:
    parts = [prefix] + parts
  return '_'.join(parts)


def main():
  parser = argparse.ArgumentParser(description='Pull format files.')
  parser.add_argument(
      '-p', dest='prefix', default=None, help='the output directory prefix')
  parser.add_argument(
      '-s',
      dest='serial',
      default=None,
      help='use device with the given serial')
  args = parser.parse_args()

  prefix = args.prefix
  serial = args.serial

  serial = ensure_single_device(serial)
  output_directory = get_output_directory(prefix, serial)

  ensure_output_directory_empty(output_directory)
  pull_format_files(serial, output_directory)

  return 0


if __name__ == '__main__':
  sys.exit(main())
