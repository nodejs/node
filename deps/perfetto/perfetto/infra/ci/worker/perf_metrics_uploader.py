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

import argparse
import json
import hashlib
import sys

from config import DB, PROJECT
from common_utils import req, SCOPES
'''
Uploads the performance metrics of the Perfetto tests to StackDriver and
Firebase.

The expected format of the JSON is as follows:
{
  metrics: [
    {
      'metric': *metric name*,
      'value': *metric value*,
      'unit': *either s (seconds) or b (bytes)*,
      'tags': {
        *tag name*: *tag value*,
        ...
      },
      'labels': {
        *label name*: *label value*,
        ...
      }
    },
    ...
  ]
}
'''

STACKDRIVER_API = 'https://monitoring.googleapis.com/v3/projects/%s' % PROJECT
SCOPES.append('https://www.googleapis.com/auth/firebase.database')
SCOPES.append('https://www.googleapis.com/auth/userinfo.email')
SCOPES.append('https://www.googleapis.com/auth/monitoring.write')


def sha1(obj):
  hasher = hashlib.sha1()
  hasher.update(json.dumps(obj, sort_keys=True, separators=(',', ':')))
  return hasher.hexdigest()


def metric_list_to_hash_dict(raw_metrics):
  metrics = {}
  for metric in raw_metrics:
    key = '%s-%s' % (metric['metric'], sha1(metric['tags']))
    metrics[key] = metric
  return metrics


def create_stackdriver_metrics(ts, metrics):
  desc = {'timeSeries': []}
  for _, metric in metrics.iteritems():
    metric_name = metric['metric']
    desc['timeSeries'] += [{
        'metric': {
            'type':
                'custom.googleapis.com/perfetto-ci/perf/%s' % metric_name,
            'labels':
                dict(
                    list(metric.get('tags', {}).items()) +
                    list(metric.get('labels', {}).items())),
        },
        'resource': {
            'type': 'global'
        },
        'points': [{
            'interval': {
                'endTime': ts
            },
            'value': {
                'doubleValue': str(metric['value'])
            }
        }]
    }]
  return desc


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--job-id',
      type=str,
      required=True,
      help='The Perfetto CI job ID to tie this upload to')
  parser.add_argument(
      'metrics_file', type=str, help='File containing the metrics to upload')
  args = parser.parse_args()

  with open(args.metrics_file, 'r') as metrics_file:
    raw_metrics = json.loads(metrics_file.read())

  job = req('GET', '%s/jobs/%s.json' % (DB, args.job_id))
  ts = job['time_started']

  metrics = metric_list_to_hash_dict(raw_metrics['metrics'])
  req('PUT', '%s/perf/%s.json' % (DB, args.job_id), body=metrics)

  # Only upload Stackdriver metrics for post-submit runs.
  git_ref = job['env'].get('PERFETTO_TEST_GIT_REF')
  if git_ref == 'refs/heads/master':
    sd_metrics = create_stackdriver_metrics(ts, metrics)
    req('POST', STACKDRIVER_API + '/timeSeries', body=sd_metrics)

  return 0


if __name__ == '__main__':
  sys.exit(main())
