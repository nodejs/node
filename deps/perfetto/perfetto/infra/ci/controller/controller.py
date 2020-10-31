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

import logging
import re
import time
import urllib

from datetime import datetime, timedelta
from google.appengine.api import taskqueue

import webapp2

from common_utils import req, utc_now_iso, parse_iso_time, SCOPES
from config import DB, GERRIT_HOST, GERRIT_PROJECT, GERRIT_POLL_SEC, PROJECT
from config import CI_SITE, GERRIT_VOTING_ENABLED, JOB_CONFIGS, LOGS_TTL_DAYS
from config import TRUSTED_EMAILS, GCS_ARTIFACTS, JOB_TIMEOUT_SEC
from config import CL_TIMEOUT_SEC
from stackdriver_metrics import STACKDRIVER_METRICS

STACKDRIVER_API = 'https://monitoring.googleapis.com/v3/projects/%s' % PROJECT

SCOPES.append('https://www.googleapis.com/auth/firebase.database')
SCOPES.append('https://www.googleapis.com/auth/userinfo.email')
SCOPES.append('https://www.googleapis.com/auth/datastore')
SCOPES.append('https://www.googleapis.com/auth/monitoring')
SCOPES.append('https://www.googleapis.com/auth/monitoring.write')

last_tick = 0

# ------------------------------------------------------------------------------
# Misc utility functions
# ------------------------------------------------------------------------------


def defer(action, **kwargs):
  '''Appends a task to the deferred queue.

  Each task will become a new HTTP request made by the AppEngine service.
  This pattern is used extensively here for several reasons:
  - Auditability in logs: it's easier to scrape logs and debug.
  - Stability: an exception fails only the current task not the whole function.
  - Reliability: The AppEngine runtime will retry failed tasks with exponential
    backoff.
  - Performance: tasks are run concurrently, which is quite important given that
    most of them are bound by HTTP latency to Gerrit of Firebase.
  '''
  taskqueue.add(
      queue_name='deferred-jobs',
      url='/controller/' + action,
      params=kwargs,
      method='GET')


def create_stackdriver_metric_definitions():
  logging.info('Creating Stackdriver metric definitions')
  for name, metric in STACKDRIVER_METRICS.iteritems():
    logging.info('Creating metric %s', name)
    req('POST', STACKDRIVER_API + '/metricDescriptors', body=metric)


def write_metrics(metric_dict):
  now = utc_now_iso()
  desc = {'timeSeries': []}
  for key, spec in metric_dict.iteritems():
    desc['timeSeries'] += [{
        'metric': {
            'type': STACKDRIVER_METRICS[key]['type'],
            'labels': spec.get('l', {})
        },
        'resource': {
            'type': 'global'
        },
        'points': [{
            'interval': {
                'endTime': now
            },
            'value': {
                'int64Value': str(spec['v'])
            }
        }]
    }]
  try:
    req('POST', STACKDRIVER_API + '/timeSeries', body=desc)
  except Exception as e:
    # Metric updates can easily fail due to Stackdriver API limitations.
    msg = str(e)
    if 'written more frequently than the maximum sampling' not in msg:
      logging.error('Metrics update failed: %s', msg)


def is_trusted(email):
  return re.match(TRUSTED_EMAILS, email)


# ------------------------------------------------------------------------------
# Deferred job handlers
# ------------------------------------------------------------------------------


def start(handler):
  create_stackdriver_metric_definitions()
  tick(handler)


def tick(handler):
  global last_tick
  now = time.time()
  # Avoid avalanching effects due to the failsafe tick job in cron.yaml.
  if now - last_tick < GERRIT_POLL_SEC - 1:
    return
  taskqueue.add(
      url='/controller/tick', queue_name='tick', countdown=GERRIT_POLL_SEC)
  defer('check_new_cls')
  defer('check_pending_cls')
  defer('update_queue_metrics')
  last_tick = now


def check_new_cls(handler):
  ''' Poll for new CLs and asynchronously enqueue jobs for them.'''
  logging.info('Polling for new Gerrit CLs')
  date_limit = (datetime.utcnow() - timedelta(days=1)).strftime('%Y-%m-%d')
  url = 'https://%s/a/changes/' % GERRIT_HOST
  url += '?o=CURRENT_REVISION&o=DETAILED_ACCOUNTS&o=LABELS&n=200'
  url += '&q=branch:master+project:%s' % GERRIT_PROJECT
  url += '+is:open+after:%s' % date_limit
  resp = req('GET', url, gerrit=True)
  for change in (change for change in resp if 'revisions' in change):
    rev_hash = change['revisions'].keys()[0]
    rev = change['revisions'][rev_hash]
    owner = rev['uploader']['email']
    prs_ready = change['labels'].get('Presubmit-Ready', {}).get('approved', {})
    prs_owner = prs_ready.get('email', '')
    # Only submit jobs for patchsets that are either uploaded by a trusted
    # account or are marked as Presubmit-Verified by a trustd account.
    if not is_trusted(owner) and not is_trusted(prs_owner):
      continue
    defer(
        'check_new_cl',
        cl=str(change['_number']),
        patchset=str(rev['_number']),
        change_id=change['id'],
        rev_hash=rev_hash,
        ref=rev['ref'],
        owner=rev['uploader']['email'],
        wants_vote='1' if prs_ready else '0')


def append_jobs(patch_obj, src, git_ref, now=None):
  '''Creates the worker jobs (defined in config.py) for the given CL.

  Jobs are keyed by timestamp-cl-patchset-config to get a fair schedule (workers
  pull jobs ordered by the key above).
  It dosn't directly write into the DB, it just appends keys to the passed
  |patch_obj|, so the whole set of CL descriptor + jobs can be added atomically
  to the datastore.
  src: is cls/1234/1 (cl and patchset number).
  '''
  logging.info('Enqueueing jobs fos cl %s', src)
  timestamp = (now or datetime.utcnow()).strftime('%Y%m%d%H%M%S')
  for cfg_name, env in JOB_CONFIGS.iteritems():
    job_id = '%s--%s--%s' % (timestamp, src.replace('/', '-'), cfg_name)
    logging.info('Enqueueing job %s', job_id)
    patch_obj['jobs/' + job_id] = {
        'src': src,
        'type': cfg_name,
        'env': dict(env, PERFETTO_TEST_GIT_REF=git_ref),
        'status': 'QUEUED',
        'time_queued': utc_now_iso(),
    }
    patch_obj['jobs_queued/' + job_id] = 0
    patch_obj[src]['jobs'][job_id] = 0


def check_new_cl(handler):
  '''Creates the CL + jobs entries in the DB for the given CL if doesn't exist

  If exists check if a Presubmit-Ready label has been added and if so updates it
  with the message + vote.
  '''
  change_id = handler.request.get('change_id')
  rev_hash = handler.request.get('rev_hash')
  cl = handler.request.get('cl')
  patchset = handler.request.get('patchset')
  ref = handler.request.get('ref')
  wants_vote = handler.request.get('wants_vote') == '1'

  # We want to do two things here:
  # 1) If the CL doesn't exist (hence vote_prop is None) carry on below and
  #    enqueue jobs for it.
  # 2) If the CL exists, we don't need to kick new jobs. However, the user
  #    might have addeed a Presubmit-Ready label after we created the CL. In
  #    this case update the |wants_vote| flag and return.
  vote_prop = req('GET', '%s/cls/%s-%s/wants_vote.json' % (DB, cl, patchset))
  if vote_prop is not None:
    if vote_prop != wants_vote and wants_vote:
      logging.info('Updating wants_vote flag on %s-%s', cl, patchset)
      req('PUT', '%s/cls/%s-%s/wants_vote.json' % (DB, cl, patchset), body=True)
      # If the label is applied after we have finished running all the jobs just
      # jump straight to the voting.
      defer('check_pending_cl', cl_and_ps='%s-%s' % (cl, patchset))
    return

  # This is the first time we see this patchset, enqueue jobs for it.

  # Dequeue jobs for older patchsets, if any.
  defer('cancel_older_jobs', cl=cl, patchset=patchset)

  src = 'cls/%s-%s' % (cl, patchset)
  # Enqueue jobs for the latest patchset.
  patch_obj = {}
  patch_obj['cls_pending/%s-%s' % (cl, patchset)] = 0
  patch_obj[src] = {
      'change_id': change_id,
      'revision_id': rev_hash,
      'time_queued': utc_now_iso(),
      'jobs': {},
      'wants_vote': wants_vote,
  }
  append_jobs(patch_obj, src, ref)
  req('PATCH', DB + '.json', body=patch_obj)


def cancel_older_jobs(handler):
  cl = handler.request.get('cl')
  patchset = handler.request.get('patchset')
  first_key = '%s-0' % cl
  last_key = '%s-z' % cl
  filt = 'orderBy="$key"&startAt="%s"&endAt="%s"' % (first_key, last_key)
  cl_objs = req('GET', '%s/cls.json?%s' % (DB, filt)) or {}
  for cl_and_ps, cl_obj in cl_objs.iteritems():
    ps = int(cl_and_ps.split('-')[-1])
    if cl_obj.get('time_ended') or ps >= int(patchset):
      continue
    logging.info('Cancelling jobs for previous patchset %s', cl_and_ps)
    map(lambda x: defer('cancel_job', job_id=x), cl_obj['jobs'].keys())


def check_pending_cls(handler):
  # Check if any pending CL has completed (all jobs are done). If so publish
  # the comment and vote on the CL.
  pending_cls = req('GET', '%s/cls_pending.json' % DB) or {}
  for cl_and_ps, _ in pending_cls.iteritems():
    defer('check_pending_cl', cl_and_ps=cl_and_ps)


def check_pending_cl(handler):
  # This function can be called twice on the same CL, e.g., in the case when the
  # Presubmit-Ready label is applied after we have finished running all the
  # jobs (we run presubmit regardless, only the voting is conditioned by PR).
  cl_and_ps = handler.request.get('cl_and_ps')
  cl_obj = req('GET', '%s/cls/%s.json' % (DB, cl_and_ps))
  all_jobs = cl_obj.get('jobs', {}).keys()
  pending_jobs = []
  for job_id in all_jobs:
    job_status = req('GET', '%s/jobs/%s/status.json' % (DB, job_id))
    pending_jobs += [job_id] if job_status in ('QUEUED', 'STARTED') else []

  if pending_jobs:
    # If the CL has been pending for too long cancel all its jobs. Upon the next
    # scan it will be deleted and optionally voted on.
    t_queued = parse_iso_time(cl_obj['time_queued'])
    age_sec = (datetime.utcnow() - t_queued).total_seconds()
    if age_sec > CL_TIMEOUT_SEC:
      logging.warning('Canceling %s, it has been pending for too long (%s sec)',
                      cl_and_ps, int(age_sec))
      map(lambda x: defer('cancel_job', job_id=x), pending_jobs)
    return

  logging.info('All jobs completed for CL %s', cl_and_ps)

  # Remove the CL from the pending queue and update end time.
  patch_obj = {
      'cls_pending/%s' % cl_and_ps: {},  # = DELETE
      'cls/%s/time_ended' % cl_and_ps: cl_obj.get('time_ended', utc_now_iso()),
  }
  req('PATCH', '%s.json' % DB, body=patch_obj)
  defer('update_cl_metrics', src='cls/' + cl_and_ps)
  map(lambda x: defer('update_job_metrics', job_id=x), all_jobs)
  if cl_obj.get('wants_vote'):
    defer('comment_and_vote_cl', cl_and_ps=cl_and_ps)


def comment_and_vote_cl(handler):
  cl_and_ps = handler.request.get('cl_and_ps')
  cl_obj = req('GET', '%s/cls/%s.json' % (DB, cl_and_ps))

  if cl_obj.get('voted'):
    logging.error('Already voted on CL %s', cl_and_ps)
    return

  if not cl_obj['wants_vote'] or not GERRIT_VOTING_ENABLED:
    logging.info('Skipping voting on CL %s', cl_and_ps)
    return

  cl_vote = 1
  passed_jobs = []
  failed_jobs = {}
  ui_links = []
  cancelled = False
  for job_id in cl_obj['jobs'].keys():
    job_obj = req('GET', '%s/jobs/%s.json' % (DB, job_id))
    job_config = JOB_CONFIGS.get(job_obj['type'], {})
    if job_obj['status'] == 'CANCELLED':
      cancelled = True
    if '-ui-' in job_id:
      ui_links.append('https://storage.googleapis.com/%s/%s/ui/index.html' %
                      (GCS_ARTIFACTS, job_id))
    if job_obj['status'] == 'COMPLETED':
      passed_jobs.append(job_id)
    elif not job_config.get('SKIP_VOTING', False):
      cl_vote = -1
      failed_jobs[job_id] = job_obj['status']

  msg = ''
  if cancelled:
    msg += 'Some jobs in this CI run were cancelled. This likely happened '
    msg += 'because a new patchset has been uploaded. Skipping vote.\n'
  log_url = CI_SITE + '/#!/logs'
  if failed_jobs:
    msg += 'FAIL:\n'
    msg += ''.join([
        ' %s/%s (%s)\n' % (log_url, job_id, status)
        for (job_id, status) in failed_jobs.iteritems()
    ])
  if passed_jobs:
    msg += 'PASS:\n'
    msg += ''.join([' %s/%s\n' % (log_url, job_id) for job_id in passed_jobs])
  if ui_links:
    msg += 'Artifacts:\n' + ''.join(' %s\n' % link for link in ui_links)
  msg += 'CI page for this CL:\n'
  msg += ' https://ci.perfetto.dev/#!/cls/%s\n' % cl_and_ps.split('-')[0]
  body = {'labels': {}, 'message': msg}
  if not cancelled:
    body['labels']['Code-Review'] = cl_vote
  logging.info('Posting results for CL %s', cl_and_ps)
  url = 'https://%s/a/changes/%s/revisions/%s/review' % (
      GERRIT_HOST, cl_obj['change_id'], cl_obj['revision_id'])
  req('POST', url, body=body, gerrit=True)
  req('PUT', '%s/cls/%s/voted.json' % (DB, cl_and_ps), body=True)


def queue_postsubmit_jobs(handler):
  '''Creates the jobs entries in the DB for the given branch or revision

  Can be called in two modes:
    1. ?branch=master: Will retrieve the SHA1 of master and call the one below.
    2. ?branch=master&rev=deadbeef1234: queues jobs for the given revision.
  '''
  prj = urllib.quote(GERRIT_PROJECT, '')
  branch = handler.request.get('branch')
  revision = handler.request.get('revision')
  assert branch

  if not revision:
    # Get the commit SHA1 of the head of the branch.
    url = 'https://%s/a/projects/%s/branches/%s' % (GERRIT_HOST, prj, branch)
    revision = req('GET', url, gerrit=True)['revision']
    assert revision
    defer('queue_postsubmit_jobs', branch=branch, revision=revision)
    return

  # Get the committer datetime for the given revision.
  url = 'https://%s/a/projects/%s/commits/%s' % (GERRIT_HOST, prj, revision)
  commit_info = req('GET', url, gerrit=True)
  time_committed = commit_info['committer']['date'].split('.')[0]
  time_committed = datetime.strptime(time_committed, '%Y-%m-%d %H:%M:%S')

  # Enqueue jobs.
  src = 'branches/%s-%s' % (branch, time_committed.strftime('%Y%m%d%H%M%S'))
  now = datetime.utcnow()
  patch_obj = {
      src: {
          'rev': revision,
          'subject': commit_info['subject'][:100],
          'author': commit_info['author'].get('email', 'N/A'),
          'time_committed': utc_now_iso(time_committed),
          'time_queued': utc_now_iso(),
          'jobs': {},
      }
  }
  ref = 'refs/heads/' + branch
  append_jobs(patch_obj, src, ref, now)
  req('PATCH', DB + '.json', body=patch_obj)


def delete_stale_jobs(handler):
  '''Deletes jobs that are left in the running queue for too long

  This is usually due to a crash in the VM that handles them.
  '''
  running_jobs = req('GET', '%s/jobs_running.json?shallow=true' % (DB)) or {}
  for job_id in running_jobs.iterkeys():
    job = req('GET', '%s/jobs/%s.json' % (DB, job_id))
    time_started = parse_iso_time(job.get('time_started', utc_now_iso()))
    age = (datetime.now() - time_started).total_seconds()
    if age > JOB_TIMEOUT_SEC * 2:
      defer('cancel_job', job_id=job_id)


def cancel_job(handler):
  '''Cancels a job if not completed or failed.

  This function is racy: workers can complete the queued jobs while we mark them
  as cancelled. The result of such race is still acceptable.'''
  job_id = handler.request.get('job_id')
  status = req('GET', '%s/jobs/%s/status.json' % (DB, job_id))
  patch_obj = {
      'jobs_running/%s' % job_id: {},  # = DELETE,
      'jobs_queued/%s' % job_id: {},  # = DELETE,
  }
  if status in ('QUEUED', 'STARTED'):
    patch_obj['jobs/%s/status' % job_id] = 'CANCELLED'
    patch_obj['jobs/%s/time_ended' % job_id] = utc_now_iso()
  req('PATCH', DB + '.json', body=patch_obj)


def delete_expired_logs(handler):
  logs = req('GET', '%s/logs.json?shallow=true' % (DB)) or {}
  for job_id in logs.iterkeys():
    age_days = (datetime.now() - datetime.strptime(job_id[:8], '%Y%m%d')).days
    if age_days > LOGS_TTL_DAYS:
      defer('delete_job_logs', job_id=job_id)


def delete_job_logs(handler):
  req('DELETE', '%s/logs/%s.json' % (DB, handler.request.get('job_id')))


def update_cl_metrics(handler):
  cl_obj = req('GET', '%s/%s.json' % (DB, handler.request.get('src')))
  t_queued = parse_iso_time(cl_obj['time_queued'])
  t_ended = parse_iso_time(cl_obj['time_ended'])
  write_metrics({
      'ci_cl_completion_time': {
          'l': {},
          'v': int((t_ended - t_queued).total_seconds())
      }
  })


def update_job_metrics(handler):
  job_id = handler.request.get('job_id')
  job = req('GET', '%s/jobs/%s.json' % (DB, job_id))
  metrics = {}
  if 'time_queued' in job and 'time_started' in job:
    t_queued = parse_iso_time(job['time_queued'])
    t_started = parse_iso_time(job['time_started'])
    metrics['ci_job_queue_time'] = {
        'l': {
            'job_type': job['type']
        },
        'v': int((t_started - t_queued).total_seconds())
    }
  if 'time_ended' in job and 'time_started' in job:
    t_started = parse_iso_time(job['time_started'])
    t_ended = parse_iso_time(job['time_ended'])
    metrics['ci_job_run_time'] = {
        'l': {
            'job_type': job['type']
        },
        'v': int((t_ended - t_started).total_seconds())
    }
  if metrics:
    write_metrics(metrics)


def update_queue_metrics(handler):
  # Update the stackdriver metric that will drive the autoscaler.
  queued = req('GET', DB + '/jobs_queued.json?shallow=true') or {}
  running = req('GET', DB + '/jobs_running.json?shallow=true') or {}
  write_metrics({'ci_job_queue_len': {'v': len(queued) + len(running)}})


class ControllerHandler(webapp2.RequestHandler):
  ACTIONS = {
      'start': start,
      'tick': tick,
      'check_pending_cls': check_pending_cls,
      'check_pending_cl': check_pending_cl,
      'check_new_cls': check_new_cls,
      'check_new_cl': check_new_cl,
      'comment_and_vote_cl': comment_and_vote_cl,
      'cancel_older_jobs': cancel_older_jobs,
      'queue_postsubmit_jobs': queue_postsubmit_jobs,
      'update_job_metrics': update_job_metrics,
      'update_queue_metrics': update_queue_metrics,
      'update_cl_metrics': update_cl_metrics,
      'delete_expired_logs': delete_expired_logs,
      'delete_job_logs': delete_job_logs,
      'delete_stale_jobs': delete_stale_jobs,
      'cancel_job': cancel_job,
  }

  def handle(self, action):
    if action in ControllerHandler.ACTIONS:
      return ControllerHandler.ACTIONS[action](self)
    raise Exception('Invalid request %s' % action)

  get = handle
  post = handle


app = webapp2.WSGIApplication([
    ('/_ah/(start)', ControllerHandler),
    (r'/controller/(\w+)', ControllerHandler),
],
                              debug=True)
