#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run a pinned gsutil."""


import argparse
import base64
import contextlib
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib2
import zipfile


GSUTIL_URL = 'https://storage.googleapis.com/pub/'
API_URL = 'https://www.googleapis.com/storage/v1/b/pub/o/'

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BIN_DIR = os.path.join(THIS_DIR, 'external_bin', 'gsutil')
DEFAULT_FALLBACK_GSUTIL = os.path.join(
    THIS_DIR, 'third_party', 'gsutil', 'gsutil')

class InvalidGsutilError(Exception):
  pass


def download_gsutil(version, target_dir):
  """Downloads gsutil into the target_dir."""
  filename = 'gsutil_%s.zip' % version
  target_filename = os.path.join(target_dir, filename)

  # Check if the target exists already.
  if os.path.exists(target_filename):
    md5_calc = hashlib.md5()
    with open(target_filename, 'rb') as f:
      while True:
        buf = f.read(4096)
        if not buf:
          break
        md5_calc.update(buf)
    local_md5 = md5_calc.hexdigest()

    metadata_url = '%s%s' % (API_URL, filename)
    metadata = json.load(urllib2.urlopen(metadata_url))
    remote_md5 = base64.b64decode(metadata['md5Hash'])

    if local_md5 == remote_md5:
      return target_filename
    os.remove(target_filename)

  # Do the download.
  url = '%s%s' % (GSUTIL_URL, filename)
  u = urllib2.urlopen(url)
  with open(target_filename, 'wb') as f:
    while True:
      buf = u.read(4096)
      if not buf:
        break
      f.write(buf)
  return target_filename


def check_gsutil(gsutil_bin):
  """Run gsutil version and make sure it runs."""
  return subprocess.call(
      [sys.executable, gsutil_bin, 'version'],
      stdout=subprocess.PIPE, stderr=subprocess.STDOUT) == 0

@contextlib.contextmanager
def temporary_directory(base):
  tmpdir = tempfile.mkdtemp(prefix='gsutil_py', dir=base)
  try:
    yield tmpdir
  finally:
    if os.path.isdir(tmpdir):
      shutil.rmtree(tmpdir)

def ensure_gsutil(version, target, clean):
  bin_dir = os.path.join(target, 'gsutil_%s' % version)
  gsutil_bin = os.path.join(bin_dir, 'gsutil', 'gsutil')
  if not clean and os.path.isfile(gsutil_bin) and check_gsutil(gsutil_bin):
    # Everything is awesome! we're all done here.
    return gsutil_bin

  if not os.path.exists(target):
    os.makedirs(target)
  with temporary_directory(target) as instance_dir:
    # Clean up if we're redownloading a corrupted gsutil.
    cleanup_path = os.path.join(instance_dir, 'clean')
    try:
      os.rename(bin_dir, cleanup_path)
    except (OSError, IOError):
      cleanup_path = None
    if cleanup_path:
      shutil.rmtree(cleanup_path)

    download_dir = os.path.join(instance_dir, 'download')
    target_zip_filename = download_gsutil(version, instance_dir)
    with zipfile.ZipFile(target_zip_filename, 'r') as target_zip:
      target_zip.extractall(download_dir)

    try:
      os.rename(download_dir, bin_dir)
    except (OSError, IOError):
      # Something else did this in parallel.
      pass

  # Final check that the gsutil bin is okay.  This should never fail.
  if not check_gsutil(gsutil_bin):
    raise InvalidGsutilError()
  return gsutil_bin


def run_gsutil(force_version, fallback, target, args, clean=False):
  if force_version:
    gsutil_bin = ensure_gsutil(force_version, target, clean)
  else:
    gsutil_bin = fallback
  cmd = [sys.executable, gsutil_bin] + args
  return subprocess.call(cmd)


def parse_args():
  bin_dir = os.environ.get('DEPOT_TOOLS_GSUTIL_BIN_DIR', DEFAULT_BIN_DIR)

  parser = argparse.ArgumentParser()
  parser.add_argument('--force-version', default='4.13')
  parser.add_argument('--clean', action='store_true',
      help='Clear any existing gsutil package, forcing a new download.')
  parser.add_argument('--fallback', default=DEFAULT_FALLBACK_GSUTIL)
  parser.add_argument('--target', default=bin_dir,
      help='The target directory to download/store a gsutil version in. '
           '(default is %(default)s).')
  parser.add_argument('args', nargs=argparse.REMAINDER)

  args, extras = parser.parse_known_args()
  if args.args and args.args[0] == '--':
    args.args.pop(0)
  if extras:
    args.args = extras + args.args
  return args


def main():
  args = parse_args()
  return run_gsutil(args.force_version, args.fallback, args.target, args.args,
                    clean=args.clean)

if __name__ == '__main__':
  sys.exit(main())
