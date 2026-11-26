# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes in the flakes.pyl config.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import ast
import re
import sys

USE_PYTHON3 = True

REQUIRED_FLAKO_PROPS = [
    'bisect_buildername',
    'bisect_builder_group',
    'bug_url',
    'extra_args',
    'isolated_name',
    'num_shards',
    'repetitions',
    'swarming_dimensions',
    'test_name',
    'timeout_sec',
    'total_timeout_sec',
    'variant',
]

LIST_FLAKO_PROPS = [
    'extra_args',
    'swarming_dimensions',
]

FORBIDDEN_FLAKO_PROPS = [
    'repro_only',
    'swarming_priority',
    'to_revision',
    'max_calibration_attempts',
]

BUG_URL_REGEXP = re.compile(r'^https?://([^/]+\.)+[^/]+(?:/?|[/?]\S+)$',
                            re.IGNORECASE)


def _ValidateFlakesPyl(input_api):
  flakes_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                       'flakes.pyl')
  try:
    with open(flakes_path, 'r') as f:
      config = ast.literal_eval(f.read())
  except:  # we can't use Exception here since it does not capture SyntaxError
    return ['failed to parse flakes.pyl: %s' % sys.exc_info()[1]]

  if (not isinstance(config, list) or
      any(not isinstance(entry, dict) for entry in config)):
    return ['top-level literal must be a list of dicts']

  errors = []
  for entry in config:
    for key in REQUIRED_FLAKO_PROPS:
      if key not in entry:
        errors.append('missing required key `%s` in %s' % (key, entry))
        continue

      if key in LIST_FLAKO_PROPS:
        if not isinstance(entry[key], list):
          errors.append('value for `%s` must be a list, got %s' %
                        (key, repr(entry[key])))
      else:
        if not isinstance(entry[key], (str, int)):
          errors.append('value for `%s` must be an int or a string, got %s' %
                        (key, repr(entry[key])))

    for key in FORBIDDEN_FLAKO_PROPS:
      if key in entry:
        errors.append('forbidden key `%s` present in %s' % (key, entry))

    if not BUG_URL_REGEXP.match(entry['bug_url']):
      errors.append('invalid URL for bug_url: %s' % entry['bug_url'])

  return errors


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  return [output_api.PresubmitError(e) for e in _ValidateFlakesPyl(input_api)]


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results
