# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True


def _CheckTrialsConfig(input_api, output_api):
  def FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=(r'.+clusterfuzz_trials_config\.json',))

  results = []
  for f in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    with open(f.AbsoluteLocalPath()) as j:
      try:
        trials = json.load(j)
        mandatory_properties = {'app_args', 'app_name', 'probability'}
        optional_properties = {'contradicts'}
        all_properties = mandatory_properties.union(optional_properties)
        for trial in trials:
          trial_keys = set(trial.keys())
          if not mandatory_properties.issubset(trial_keys):
            results.append('trial {} does not have mandatory propertie(s) {}'.format(trial, mandatory_properties - trial_keys))
          if not trial_keys.issubset(all_properties):
            results.append('trial {} has incorrect propertie(s) {}'.format(trial, trial_keys - all_properties))
          if trial['app_name'] != 'd8':
            results.append('trial {} has an incorrect app_name'.format(trial))
          if not isinstance(trial['probability'], float):
            results.append('trial {} probability is not a float'.format(trial))
          if not (0 <= trial['probability'] <= 1):
            results.append(
                'trial {} has invalid probability value'.format(trial))
          if not isinstance(trial['app_args'], str) or not trial['app_args']:
            results.append(
                'trial {} should have a non-empty string for app_args'.format(
                    trial))
          contradicts = trial.get('contradicts', [])
          if not isinstance(contradicts, list) or not all(
              isinstance(cont, str) for cont in contradicts):
              results.append(
                'trial {} contradicts is not a list of strings'.format(trial))
      except Exception as e:
        results.append(
            'JSON validation failed for %s. Error:\n%s' % (f.LocalPath(), e))

  return [output_api.PresubmitError(r) for r in results]

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  checks = [
    _CheckTrialsConfig,
  ]

  return sum([check(input_api, output_api) for check in checks], [])

def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)

def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)
