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
""" Mirrors a Gerrit repo into GitHub.

Mirrors all the branches (refs/heads/foo) from Gerrit to Github as-is, taking
care of propagating also deletions.

This script used to be more complex, turning all the Gerrit CLs
(refs/changes/NN/cl_number/patchset_number) into Github branches
(refs/heads/cl_number). This use case was dropped as we moved away from Travis.
See the git history of this file for more.
"""

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
import time

CUR_DIR = os.path.dirname(os.path.abspath(__file__))
GIT_UPSTREAM = 'https://android.googlesource.com/platform/external/perfetto/'
GIT_MIRROR = 'git@github.com:google/perfetto.git'
WORKDIR = os.path.join(CUR_DIR, 'repo')

# Min delay (in seconds) between two consecutive git poll cycles. This is to
# avoid hitting gerrit API quota limits.
POLL_PERIOD_SEC = 60

# The actual key is stored into the Google Cloud project metadata.
ENV = {'GIT_SSH_COMMAND': 'ssh -i ' + os.path.join(CUR_DIR, 'deploy_key')}


def GitCmd(*args, **kwargs):
  cmd = ['git'] + list(args)
  p = subprocess.Popen(
      cmd,
      stdin=subprocess.PIPE,
      stdout=subprocess.PIPE,
      stderr=sys.stderr,
      cwd=WORKDIR,
      env=ENV)
  out = p.communicate(kwargs.get('stdin'))[0]
  assert p.returncode == 0, 'FAIL: ' + ' '.join(cmd)
  return out


# Create a git repo that mirrors both the upstream and the mirror repos.
def Setup(args):
  if os.path.exists(WORKDIR):
    if args.no_clean:
      return
    shutil.rmtree(WORKDIR)
  os.makedirs(WORKDIR)
  GitCmd('init', '--bare', '--quiet')
  GitCmd('remote', 'add', 'upstream', GIT_UPSTREAM)
  GitCmd('config', 'remote.upstream.tagOpt', '--no-tags')
  GitCmd('config', '--add', 'remote.upstream.fetch',
         '+refs/heads/*:refs/remotes/upstream/heads/*')
  GitCmd('config', '--add', 'remote.upstream.fetch',
         '+refs/tags/*:refs/remotes/upstream/tags/*')
  GitCmd('remote', 'add', 'mirror', GIT_MIRROR, '--mirror=fetch')


def Sync(args):
  logging.info('Fetching git remotes')
  GitCmd('fetch', '--all', '--quiet')
  all_refs = GitCmd('show-ref')
  future_heads = {}
  current_heads = {}

  # List all refs from both repos and:
  # 1. Keep track of all branch heads refnames and sha1s from the (github)
  #    mirror into |current_heads|.
  # 2. Keep track of all upstream (AOSP) branch heads into |future_heads|. Note:
  #    this includes only pure branches and NOT CLs. CLs and their patchsets are
  #    stored in a hidden ref (refs/changes) which is NOT under refs/heads.
  # 3. Keep track of all upstream (AOSP) CLs from the refs/changes namespace
  #    into changes[cl_number][patchset_number].
  for line in all_refs.splitlines():
    ref_sha1, ref = line.split()

    FILTER_REGEX = r'(heads/master|heads/releases/.*|tags/v\d+\.\d+)$'
    m = re.match('refs/' + FILTER_REGEX, ref)
    if m is not None:
      branch = m.group(1)
      current_heads['refs/' + branch] = ref_sha1
      continue

    m = re.match('refs/remotes/upstream/' + FILTER_REGEX, ref)
    if m is not None:
      branch = m.group(1)
      future_heads['refs/' + branch] = ref_sha1
      continue

  deleted_heads = set(current_heads) - set(future_heads)
  logging.info('current_heads: %d, future_heads: %d, deleted_heads: %d',
               len(current_heads), len(future_heads), len(deleted_heads))

  # Now compute:
  # 1. The set of branches in the mirror (github) that have been deleted on the
  #    upstream (AOSP) repo. These will be deleted also from the mirror.
  # 2. The set of rewritten branches to be updated.
  update_ref_cmd = ''
  for ref_to_delete in deleted_heads:
    update_ref_cmd += 'delete %s\n' % ref_to_delete
  for ref_to_update, ref_sha1 in future_heads.iteritems():
    if current_heads.get(ref_to_update) != ref_sha1:
      update_ref_cmd += 'update %s %s\n' % (ref_to_update, ref_sha1)

  GitCmd('update-ref', '--stdin', stdin=update_ref_cmd)

  if args.push:
    logging.info('Pushing updates')
    GitCmd('push', 'mirror', '--all', '--prune', '--force', '--follow-tags')
    GitCmd('gc', '--prune=all', '--aggressive', '--quiet')
  else:
    logging.info('Dry-run mode, skipping git push. Pass --push for prod mode.')


def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--push', default=False, action='store_true')
  parser.add_argument('--no-clean', default=False, action='store_true')
  parser.add_argument('-v', dest='verbose', default=False, action='store_true')
  args = parser.parse_args()

  logging.basicConfig(
      format='%(asctime)s %(levelname)-8s %(message)s',
      level=logging.DEBUG if args.verbose else logging.INFO,
      datefmt='%Y-%m-%d %H:%M:%S')

  logging.info('Setting up git repo one-off')
  Setup(args)
  while True:
    logging.info('------- BEGINNING OF SYNC CYCLE -------')
    Sync(args)
    logging.info('------- END OF SYNC CYCLE -------')
    time.sleep(POLL_PERIOD_SEC)


if __name__ == '__main__':
  sys.exit(Main())
