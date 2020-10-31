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
''' Worker main loop. Pulls jobs from the DB and runs them in the sandbox

It also handles timeouts and graceful container termination.
'''

import logging
import os
import random
import signal
import socket
import subprocess
import threading
import time
import traceback

from config import DB, JOB_TIMEOUT_SEC
from common_utils import req, utc_now_iso, init_logging
from common_utils import ConcurrentModificationError, SCOPES

CUR_DIR = os.path.dirname(__file__)
SCOPES.append('https://www.googleapis.com/auth/firebase.database')
SCOPES.append('https://www.googleapis.com/auth/userinfo.email')
WORKER_NAME = '%s-%s' % (os.getenv('WORKER_HOST', 'local').split('-')[-1],
                         socket.gethostname())
sigterm = threading.Event()


def try_acquire_job(job_id):
  ''' Transactionally acquire the given job.

  Returns the job JSON object if it managed to acquire and put it into the
  STARTED state, None if another worker got there first.
  '''
  logging.debug('Trying to acquire job %s', job_id)

  uri = '%s/jobs/%s.json' % (DB, job_id)
  job, etag = req('GET', uri, req_etag=True)
  if job['status'] != 'QUEUED':
    return None  # Somebody else took it
  try:
    job['status'] = 'STARTED'
    job['time_started'] = utc_now_iso()
    job['worker'] = WORKER_NAME
    req('PUT', uri, body=job, etag=etag)
    return job
  except ConcurrentModificationError:
    return None


def make_worker_obj(status, job_id=None):
  return {
      'job_id': job_id,
      'status': status,
      'last_update': utc_now_iso(),
      'host': os.getenv('WORKER_HOST', '')
  }


def worker_loop():
  ''' Pulls a job from the queue and runs it invoking run_job.py  '''
  uri = '%s/jobs_queued.json?orderBy="$key"&limitToLast=10' % DB
  jobs = req('GET', uri)
  if not jobs:
    return

  # Transactionally acquire a job. Deal with races (two workers trying to
  # acquire the same job).
  job = None
  job_id = None
  for job_id in sorted(jobs.keys(), reverse=True):
    job = try_acquire_job(job_id)
    if job is not None:
      break
    logging.info('Raced while trying to acquire job %s, retrying', job_id)
    time.sleep(int(random.random() * 3))
  if job is None:
    logging.error('Failed to acquire a job')
    return

  logging.info('Starting job %s', job_id)

  # Update the db, move the job to the running queue.
  patch_obj = {
      'jobs_queued/' + job_id: {},  # = DELETE
      'jobs_running/' + job_id: {
          'worker': WORKER_NAME
      },
      'workers/' + WORKER_NAME: make_worker_obj('RUNNING', job_id=job_id)
  }
  req('PATCH', '%s.json' % DB, body=patch_obj)

  cmd = [os.path.join(CUR_DIR, 'run_job.py'), job_id]

  # Propagate the worker's PERFETTO_  vars and merge with the job-specific vars.
  env = dict(os.environ, **{k: str(v) for (k, v) in job['env'].iteritems()})
  job_runner = subprocess.Popen(cmd, env=env)

  # Run the job in a python subprocess, to isolate the main loop from logs
  # uploader failures.
  res = None
  cancelled = False
  timed_out = False
  time_started = time.time()
  time_last_db_poll = time_started
  polled_status = 'STARTED'
  while res is None:
    time.sleep(0.25)
    res = job_runner.poll()
    now = time.time()
    if now - time_last_db_poll > 10:  # Throttle DB polling.
      polled_status = req('GET', '%s/jobs/%s/status.json' % (DB, job_id))
      time_last_db_poll = now
    if now - time_started > JOB_TIMEOUT_SEC:
      logging.info('Job %s timed out, terminating', job_id)
      timed_out = True
      job_runner.terminate()
    if (sigterm.is_set() or polled_status != 'STARTED') and not cancelled:
      logging.info('Job %s cancelled, terminating', job_id)
      cancelled = True
      job_runner.terminate()

  status = ('INTERRUPTED' if sigterm.is_set() else 'CANCELLED' if cancelled else
            'TIMED_OUT' if timed_out else 'COMPLETED' if res == 0 else 'FAILED')
  logging.info('Job %s %s with code %s', job_id, status, res)

  # Update the DB, unless the job has been cancelled. The "is not None"
  # condition deals with a very niche case, that is, avoid creating a partial
  # job entry after doing a full clear of the DB (which is super rare, happens
  # only when re-deploying the CI).
  if polled_status is not None:
    patch = {
        'jobs/%s/status' % job_id: status,
        'jobs/%s/exit_code' % job_id: {} if res is None else res,
        'jobs/%s/time_ended' % job_id: utc_now_iso(),
        'jobs_running/%s' % job_id: {},  # = DELETE
    }
    req('PATCH', '%s.json' % (DB), body=patch)


def sig_handler(_, __):
  logging.warn('Interrupted by signal, exiting worker')
  sigterm.set()


def main():
  init_logging()
  logging.info('Worker started')
  signal.signal(signal.SIGTERM, sig_handler)
  signal.signal(signal.SIGINT, sig_handler)

  while not sigterm.is_set():
    logging.debug('Starting poll cycle')
    try:
      worker_loop()
      req('PUT',
          '%s/workers/%s.json' % (DB, WORKER_NAME),
          body=make_worker_obj('IDLE'))
    except:
      logging.error('Exception in worker loop:\n%s', traceback.format_exc())
    if sigterm.is_set():
      break
    time.sleep(5)

  # The use case here is the VM being terminated by the GCE infrastructure.
  # We mark the worker as terminated and the job as cancelled so we don't wait
  # forever for it.
  logging.warn('Exiting the worker loop, got signal: %s', sigterm.is_set())
  req('PUT',
      '%s/workers/%s.json' % (DB, WORKER_NAME),
      body=make_worker_obj('TERMINATED'))


if __name__ == '__main__':
  main()
