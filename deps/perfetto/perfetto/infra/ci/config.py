#!/usr/bin/env python
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
'''Project-wide configuration

This file is either imported from other python scripts or executed to generate
makefile dumps of the variables. This is so all vars can live in one place.
'''

from __future__ import print_function

# Gerrit config
GERRIT_HOST = 'android-review.googlesource.com'
GERRIT_PROJECT = 'platform/external/perfetto'
GERRIT_REVIEW_URL = (
    'https://android-review.googlesource.com/c/' + GERRIT_PROJECT)
REPO_URL = 'https://android.googlesource.com/' + GERRIT_PROJECT
GERRIT_POLL_SEC = 15
GERRIT_VOTING_ENABLED = True
LOGLEVEL = 'info'

# Cloud config (GCE = Google Compute Engine, GAE = Google App Engine)
PROJECT = 'perfetto-ci'
ZONE = 'us-west1-b'
GAE_VERSION = 'prod'
DB_ROOT = 'https://%s.firebaseio.com' % PROJECT
DB = DB_ROOT + '/ci'
SANDBOX_IMG = 'eu.gcr.io/%s/sandbox' % PROJECT
WORKER_IMG = 'eu.gcr.io/%s/worker' % PROJECT
CI_SITE = 'https://ci.perfetto.dev'
GCS_ARTIFACTS = 'perfetto-ci-artifacts'

JOB_TIMEOUT_SEC = 60 * 30
CL_TIMEOUT_SEC = 60 * 60 * 3
LOGS_TTL_DAYS = 15
TRUSTED_EMAILS = '^.*@google.com$'

GCE_VM_NAME = 'ci-worker'
GCE_VM_TYPE = 'e2-standard-16'
GCE_TEMPLATE = 'ci-worker-template'
GCE_GROUP_NAME = 'ci'
NUM_VMS = 6
NUM_WORKERS_PER_VM = 5

GCE_SCOPES = [
    'https://www.googleapis.com/auth/cloud-platform',
    'https://www.googleapis.com/auth/devstorage.read_write',
    'https://www.googleapis.com/auth/firebase.database',
    'https://www.googleapis.com/auth/logging.write',
    'https://www.googleapis.com/auth/monitoring.write',
    'https://www.googleapis.com/auth/trace.append',
    'https://www.googleapis.com/auth/userinfo.email',
]

# Only variables starting with PERFETTO_ are propagated into the sandbox.
JOB_CONFIGS = {
    'linux-clang-x86_64-debug': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=true is_hermetic_clang=false',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'linux-clang-x86_64-tsan': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_tsan=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'linux-clang-x86_64-msan': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_msan=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'linux-clang-x86_64-asan_lsan': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_asan=true is_lsan=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'linux-clang-x86-asan_lsan': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_asan=true is_lsan=true '
                                 'target_cpu="x86"',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'linux-gcc7-x86_64-release': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_clang=false '
                                 'cc="gcc-7" cxx="g++-7"',
        'PERFETTO_TEST_SCRIPT': 'test/ci/linux_tests.sh',
    },
    'android-clang-arm-release': {
        'PERFETTO_TEST_GN_ARGS':
            'is_debug=false target_os="android" target_cpu="arm"',
        'PERFETTO_TEST_SCRIPT':
            'test/ci/android_tests.sh',
    },
    'android-clang-arm-asan': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false target_os="android" '
                                 'target_cpu="arm" is_asan=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/android_tests.sh',
    },
    'linux-clang-x86_64-libfuzzer': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false is_fuzzer=true is_asan=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/fuzzer_tests.sh',
    },
    'linux-clang-x86_64-bazel': {
        'PERFETTO_TEST_GN_ARGS': '',
        'PERFETTO_TEST_SCRIPT': 'test/ci/bazel_tests.sh',
    },
    'ui-clang-x86_64-debug': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=true',
        'PERFETTO_TEST_SCRIPT': 'test/ci/ui_tests.sh',
    },
    'ui-clang-x86_64-release': {
        'PERFETTO_TEST_GN_ARGS': 'is_debug=false',
        'PERFETTO_TEST_SCRIPT': 'test/ci/ui_tests.sh',
    },
}

if __name__ == '__main__':
  import os
  import json
  import re
  import sys
  vars = dict(kv for kv in locals().items() if re.match('^[A-Z0-9_]+$', kv[0]))

  if len(sys.argv) > 1 and sys.argv[1] == 'makefile':
    deps_path = os.path.join(os.path.dirname(__file__), '.deps')
    if not os.path.exists(deps_path):
      os.mkdir(deps_path)
    gen_file = os.path.join(deps_path, 'config.mk')

    try:
      literals = (int, long, basestring)
    except NameError:
      literals = (int, str)

    with open(gen_file, 'w') as f:
      for k, v in vars.items():
        if isinstance(v, literals):
          f.write('override %s=%s\n' % (k, v))
        elif isinstance(v, list):
          f.write('override %s=%s\n' % (k, ','.join(v)))

    print(gen_file)

  if len(sys.argv) > 1 and sys.argv[1] == 'js':
    jsn = json.dumps(vars, indent=2)
    print('// Auto-generated by %s, do not edit.\n' % __file__)
    print('\'use strict\';\n')
    print('const cfg = JSON.parse(`%s`);\n' % jsn.replace(r'\"', r'\\\"'))
