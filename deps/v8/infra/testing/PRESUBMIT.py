# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Presubmit checks for the validity of V8-side test specifications in pyl files.

For simplicity, we check all pyl files on any changes in this folder.
"""

import ast
import os

try:
  basestring       # Python 2
except NameError:  # Python 3
  basestring = str

SUPPORTED_BUILDER_SPEC_KEYS = [
  'swarming_dimensions',
  'swarming_task_attrs',
  'tests',
]

# This is not an exhaustive list. It only reflects what we currently use. If
# there's need to specify a different dimension, just add it here.
SUPPORTED_SWARMING_DIMENSIONS = [
  'cores',
  'cpu',
  'device_os',
  'device_type',
  'os',
]

# This is not an exhaustive list. It only reflects what we currently use. If
# there's need to specify a different property, just add it here.
SUPPORTED_SWARMING_TASK_ATTRS = [
  'expiration',
  'hard_timeout',
  'priority',
]

SUPPORTED_TEST_KEYS = [
  'name',
  'shards',
  'suffix',
  'swarming_dimensions',
  'swarming_task_attrs',
  'test_args',
  'variant',
]

def check_keys(error_msg, src_dict, supported_keys):
  errors = []
  for key in src_dict.keys():
    if key not in supported_keys:
      errors += error_msg('Key "%s" must be one of %s' % (key, supported_keys))
  return errors


def _check_properties(error_msg, src_dict, prop_name, supported_keys):
  properties = src_dict.get(prop_name, {})
  if not isinstance(properties, dict):
    return error_msg('Value for %s must be a dict' % prop_name)
  return check_keys(error_msg, properties, supported_keys)


def _check_int_range(error_msg, src_dict, prop_name, lower_bound=None,
                     upper_bound=None):
  if prop_name not in src_dict:
    # All properties are optional.
    return []
  try:
    value = int(src_dict[prop_name])
  except ValueError:
    return error_msg('If specified, %s must be an int' % prop_name)
  if lower_bound is not None and value < lower_bound:
    return error_msg('If specified, %s must be >=%d' % (prop_name, lower_bound))
  if upper_bound is not None and value > upper_bound:
    return error_msg('If specified, %s must be <=%d' % (prop_name, upper_bound))
  return []


def _check_swarming_task_attrs(error_msg, src_dict):
  errors = []
  task_attrs = src_dict.get('swarming_task_attrs', {})
  errors += _check_int_range(
      error_msg, task_attrs, 'priority', lower_bound=25, upper_bound=100)
  errors += _check_int_range(
      error_msg, task_attrs, 'expiration', lower_bound=1)
  errors += _check_int_range(
      error_msg, task_attrs, 'hard_timeout', lower_bound=1)
  return errors


def _check_swarming_config(error_msg, src_dict):
  errors = []
  errors += _check_properties(
      error_msg, src_dict, 'swarming_dimensions',
      SUPPORTED_SWARMING_DIMENSIONS)
  errors += _check_properties(
      error_msg, src_dict, 'swarming_task_attrs',
      SUPPORTED_SWARMING_TASK_ATTRS)
  errors += _check_swarming_task_attrs(error_msg, src_dict)
  return errors


def _check_test(error_msg, test):
  if not isinstance(test, dict):
    return error_msg('Each test must be specified with a dict')
  errors = check_keys(error_msg, test, SUPPORTED_TEST_KEYS)
  if not test.get('name'):
    errors += error_msg('A test requires a name')
  errors += _check_swarming_config(error_msg, test)

  test_args = test.get('test_args', [])
  if not isinstance(test_args, list):
    errors += error_msg('If specified, test_args must be a list of arguments')
  if not all(isinstance(x, basestring) for x in test_args):
    errors += error_msg('If specified, all test_args must be strings')

  # Limit shards to 12 to avoid erroneous resource exhaustion.
  errors += _check_int_range(
      error_msg, test, 'shards', lower_bound=1, upper_bound=12)

  variant = test.get('variant', 'default')
  if not variant or not isinstance(variant, basestring):
    errors += error_msg('If specified, variant must be a non-empty string')

  return errors


def _check_test_spec(file_path, raw_pyl):
  def error_msg(msg):
    return ['Error in %s:\n%s' % (file_path, msg)]

  try:
    # Eval python literal file.
    full_test_spec = ast.literal_eval(raw_pyl)
  except SyntaxError as e:
    return error_msg('Pyl parsing failed with:\n%s' % e)

  if not isinstance(full_test_spec, dict):
    return error_msg('Test spec must be a dict')

  errors = []
  for buildername, builder_spec in full_test_spec.iteritems():
    def error_msg(msg):
      return ['Error in %s for builder %s:\n%s' % (file_path, buildername, msg)]

    if not isinstance(buildername, basestring) or not buildername:
      errors += error_msg('Buildername must be a non-empty string')

    if not isinstance(builder_spec, dict) or not builder_spec:
      errors += error_msg('Value must be a non-empty dict')
      continue

    errors += check_keys(error_msg, builder_spec, SUPPORTED_BUILDER_SPEC_KEYS)
    errors += _check_swarming_config(error_msg, builder_spec)

    for test in builder_spec.get('tests', []):
      errors += _check_test(error_msg, test)

  return errors



def CheckChangeOnCommit(input_api, output_api):
  def file_filter(regexp):
    return lambda f: input_api.FilterSourceFile(f, white_list=(regexp,))

  # Calculate which files are affected.
  if input_api.AffectedFiles(False, file_filter(r'.*PRESUBMIT\.py')):
    # If PRESUBMIT.py itself was changed, check also the test spec.
    affected_files = [
      os.path.join(input_api.PresubmitLocalPath(), 'builders.pyl'),
    ]
  else:
    # Otherwise, check test spec only when changed.
    affected_files = [
      f.AbsoluteLocalPath()
      for f in input_api.AffectedFiles(False, file_filter(r'.*builders\.pyl'))
    ]

  errors = []
  for file_path in affected_files:
    with open(file_path) as f:
      errors += _check_test_spec(file_path, f.read())
  return [output_api.PresubmitError(r) for r in errors]
