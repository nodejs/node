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
''' Runs the given job in an isolated docker container.

Also streams stdout/err onto the firebase realtime DB.
'''

import fcntl
import logging
import json
import os
import Queue
import signal
import socket
import shutil
import subprocess
import sys
import threading
import time

from datetime import datetime, timedelta
from oauth2client.client import GoogleCredentials
from config import DB, SANDBOX_IMG
from common_utils import init_logging, req, ConcurrentModificationError, SCOPES

CUR_DIR = os.path.dirname(__file__)
SCOPES.append('https://www.googleapis.com/auth/firebase.database')
SCOPES.append('https://www.googleapis.com/auth/userinfo.email')


def read_nonblock(fd):
  fcntl.fcntl(fd, fcntl.F_SETFL, fcntl.fcntl(fd, fcntl.F_GETFL) | os.O_NONBLOCK)
  res = ''
  while True:
    try:
      buf = os.read(fd.fileno(), 1024)
      if not buf:
        break
      res += buf
    except OSError:
      break
  return res


def log_thread(job_id, queue):
  ''' Uploads stdout/stderr from the queue to the firebase DB.

  Each line is logged as an invidivual entry in the DB, as follows:
  MMMMMM-NNNN log line, where M: hex-encodeed timestamp, N:  monotonic counter.
  '''
  uri = '%s/logs/%s.json' % (DB, job_id)
  req('DELETE', uri)
  while True:
    batch = queue.get()
    if batch is None:
      break  # EOF
    req('PATCH', uri, body=batch)
  logging.debug('Uploader thread terminated')


def main(argv):
  init_logging()
  if len(argv) != 2:
    print 'Usage: %s job_id' % argv[0]
    return 1

  job_id = argv[1]
  res = 42

  # The container name will be worker-N-sandbox.
  container = socket.gethostname() + '-sandbox'

  # Remove stale jobs, if any.
  subprocess.call(['sudo', 'docker', 'rm', '-f', container])

  q = Queue.Queue()

  # Conversely to real programs, signal handlers in python aren't really async
  # but are queued on the main thread. Hence We need to keep the main thread
  # responsive to react to signals. This is to handle timeouts and graceful
  # termination of the worker container, which dispatches a SIGTERM on stop.
  def sig_handler(sig, _):
    logging.warn('Job runner got signal %s, terminating job %s', sig, job_id)
    subprocess.call(['sudo', 'docker', 'kill', container])
    os._exit(1)  # sys.exit throws a SystemExit exception, _exit really exits.

  signal.signal(signal.SIGTERM, sig_handler)

  log_thd = threading.Thread(target=log_thread, args=(job_id, q))
  log_thd.start()

  # SYS_PTRACE is required for gtest death tests and LSan.
  cmd = [
      'sudo', 'docker', 'run', '--name', container, '--hostname', container,
      '--cap-add', 'SYS_PTRACE', '--rm', '--tmpfs', '/ci/ramdisk:exec',
      '--tmpfs', '/tmp:exec', '--env',
      'PERFETTO_TEST_JOB=%s' % job_id
  ]

  # Propagate environment variables coming from the job config.
  for kv in [kv for kv in os.environ.items() if kv[0].startswith('PERFETTO_')]:
    cmd += ['--env', '%s=%s' % kv]

  # Rationale for the conditional branches below: when running in the real GCE
  # environment, the gce-startup-script.sh mounts these directories in the right
  # locations, so that they are shared between all workers.
  # When running the worker container outside of GCE (i.e.for local testing) we
  # leave these empty. The VOLUME directive in the dockerfile will cause docker
  # to automatically mount a scratch volume for those.
  # This is so that the CI containers can be tested without having to do the
  # work that gce-startup-script.sh does.
  if os.getenv('SHARED_WORKER_CACHE'):
    cmd += ['--volume=%s:/ci/cache' % os.getenv('SHARED_WORKER_CACHE')]

  artifacts_dir = None
  if os.getenv('ARTIFACTS_DIR'):
    artifacts_dir = os.path.join(os.getenv('ARTIFACTS_DIR'), job_id)
    subprocess.call(['sudo', 'rm', '-rf', artifacts_dir])
    os.mkdir(artifacts_dir)
    cmd += ['--volume=%s:/ci/artifacts' % artifacts_dir]

  cmd += os.getenv('SANDBOX_NETWORK_ARGS', '').split()
  cmd += [SANDBOX_IMG]

  logging.info('Starting %s', ' '.join(cmd))
  proc = subprocess.Popen(
      cmd,
      stdin=open(os.devnull),
      stdout=subprocess.PIPE,
      stderr=subprocess.STDOUT,
      bufsize=65536)
  stdout = ''
  tstart = time.time()
  while True:
    ms_elapsed = int((time.time() - tstart) * 1000)
    stdout += read_nonblock(proc.stdout)

    # stdout/err pipes are not atomic w.r.t. '\n'. Extract whole lines out into
    # |olines| and keep the last partial line (-1) in the |stdout| buffer.
    lines = stdout.split('\n')
    stdout = lines[-1]
    lines = lines[:-1]

    # Each line has a key of the form <time-from-start><out|err><counter>
    # |counter| is relative to the batch and is only used to disambiguate lines
    # fetched at the same time, preserving the ordering.
    batch = {}
    for counter, line in enumerate(lines):
      batch['%06x-%04x' % (ms_elapsed, counter)] = line
    if batch:
      q.put(batch)
    if proc.poll() is not None:
      res = proc.returncode
      logging.info('Job subprocess terminated with code %s', res)
      break

    # Large sleeps favour batching in the log uploader.
    # Small sleeps favour responsiveness of the signal handler.
    time.sleep(1)

  q.put(None)  # EOF maker
  log_thd.join()

  if artifacts_dir:
    artifacts_uploader = os.path.join(CUR_DIR, 'artifacts_uploader.py')
    cmd = ['setsid', artifacts_uploader, '--job-id=%s' % job_id, '--rm']
    subprocess.call(cmd)

  return res


if __name__ == '__main__':
  sys.exit(main(sys.argv))
