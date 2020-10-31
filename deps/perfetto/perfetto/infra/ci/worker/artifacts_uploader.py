#!/usr/bin/env python2
# Copyright (C) 2019 The Android Open Source Project
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

import argparse
import httplib2
import logging
import mimetypes
import mmap
import os
import subprocess
import signal
import sys
import threading
import time

from common_utils import init_logging
from config import GCS_ARTIFACTS
from multiprocessing.pool import ThreadPool
from oauth2client.client import GoogleCredentials

CUR_DIR = os.path.dirname(__file__)
RESCAN_PERIOD_SEC = 5  # Scan for new artifact directories every X seconds.
WATCHDOG_SEC = 60 * 6  # Self kill after 5 minutes

tls = threading.local()
'''Polls for new directories under ARTIFACTS_DIR and uploads them to GCS'''


def get_http_obj():
  http = getattr(tls, 'http', None)
  if http is not None:
    return http
  tls.http = httplib2.Http()
  scopes = ['https://www.googleapis.com/auth/cloud-platform']
  creds = GoogleCredentials.get_application_default().create_scoped(scopes)
  creds.authorize(tls.http)
  return tls.http


def upload_one_file(fpath):
  http = get_http_obj()
  relpath = os.path.relpath(fpath, os.getenv('ARTIFACTS_DIR'))
  logging.debug('Uploading %s', relpath)
  assert (os.path.exists(fpath))
  fsize = os.path.getsize(fpath)
  mime_type = mimetypes.guess_type(fpath)[0] or 'application/octet-stream'
  mm = ''
  hdr = {'Content-Length': fsize, 'Content-type': mime_type}
  if fsize > 0:
    with open(fpath, 'rb') as f:
      mm = mmap.mmap(f.fileno(), fsize, access=mmap.ACCESS_READ)
  uri = 'https://%s.storage.googleapis.com/%s' % (GCS_ARTIFACTS, relpath)
  resp, res = http.request(uri, method='PUT', headers=hdr, body=mm)
  if fsize > 0:
    mm.close()
  if resp.status != 200:
    logging.error('HTTP request failed with code %d : %s', resp.status, res)
    return -1
  return fsize


def upload_one_file_with_retries(fpath):
  for retry in [0.5, 1.5, 3]:
    res = upload_one_file(fpath)
    if res >= 0:
      return res
    logging.warn('Upload of %s failed, retrying in %s seconds', fpath, retry)
    time.sleep(retry)


def list_files(path):
  for root, _, files in os.walk(path):
    for fname in files:
      fpath = os.path.join(root, fname)
      if os.path.isfile(fpath):
        yield fpath


def scan_and_upload_perf_folder(job_id, dirpath):
  perf_folder = os.path.join(dirpath, 'perf')
  if not os.path.isdir(perf_folder):
    return
  uploader = os.path.join(CUR_DIR, 'perf_metrics_uploader.py')
  for path in list_files(perf_folder):
    subprocess.call([uploader, '--job-id', job_id, path])


def main():
  init_logging()
  signal.alarm(WATCHDOG_SEC)
  mimetypes.add_type('application/wasm', '.wasm')

  parser = argparse.ArgumentParser()
  parser.add_argument('--rm', action='store_true', help='Removes the directory')
  parser.add_argument(
      '--job-id',
      type=str,
      required=True,
      help='The Perfetto CI job ID to tie this upload to')
  args = parser.parse_args()
  job_id = args.job_id
  dirpath = os.path.join(os.getenv('ARTIFACTS_DIR', default=os.curdir), job_id)
  if not os.path.isdir(dirpath):
    logging.error('Directory not found: %s', dirpath)
    return 1

  total_size = 0
  uploads = 0
  failures = 0
  files = list_files(dirpath)
  pool = ThreadPool(processes=10)
  for upl_size in pool.imap_unordered(upload_one_file_with_retries, files):
    uploads += 1 if upl_size >= 0 else 0
    failures += 1 if upl_size < 0 else 0
    total_size += max(upl_size, 0)

  logging.info('Uploaded artifacts for %s: %d files, %s failures, %d KB',
               job_id, uploads, failures, total_size / 1e3)

  scan_and_upload_perf_folder(job_id, dirpath)

  if args.rm:
    subprocess.call(['sudo', 'rm', '-rf', dirpath])

  return 0


if __name__ == '__main__':
  sys.exit(main())
