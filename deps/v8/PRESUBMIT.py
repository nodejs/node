# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Top-level presubmit script for V8.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into gcl.
"""

import sys


def _V8PresubmitChecks(input_api, output_api):
  """Runs the V8 presubmit checks."""
  import sys
  sys.path.append(input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools'))
  from presubmit import CppLintProcessor
  from presubmit import SourceProcessor
  from presubmit import CheckGeneratedRuntimeTests
  from presubmit import CheckExternalReferenceRegistration

  results = []
  if not CppLintProcessor().Run(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError("C++ lint check failed"))
  if not SourceProcessor().Run(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError(
        "Copyright header, trailing whitespaces and two empty lines " \
        "between declarations check failed"))
  if not CheckGeneratedRuntimeTests(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError(
        "Generated runtime tests check failed"))
  if not CheckExternalReferenceRegistration(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError(
        "External references registration check failed"))
  return results


def _CheckUnwantedDependencies(input_api, output_api):
  """Runs checkdeps on #include statements added in this
  change. Breaking - rules is an error, breaking ! rules is a
  warning.
  """
  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'buildtools', 'checkdeps')]
    import checkdeps
    from cpp_checker import CppChecker
    from rules import Rule
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  added_includes = []
  for f in input_api.AffectedFiles():
    if not CppChecker.IsCppFile(f.LocalPath()):
      continue

    changed_lines = [line for line_num, line in f.ChangedContents()]
    added_includes.append([f.LocalPath(), changed_lines])

  deps_checker = checkdeps.DepsChecker(input_api.PresubmitLocalPath())

  error_descriptions = []
  warning_descriptions = []
  for path, rule_type, rule_description in deps_checker.CheckAddedCppIncludes(
      added_includes):
    description_with_path = '%s\n    %s' % (path, rule_description)
    if rule_type == Rule.DISALLOW:
      error_descriptions.append(description_with_path)
    else:
      warning_descriptions.append(description_with_path)

  results = []
  if error_descriptions:
    results.append(output_api.PresubmitError(
        'You added one or more #includes that violate checkdeps rules.',
        error_descriptions))
  if warning_descriptions:
    results.append(output_api.PresubmitPromptOrNotify(
        'You added one or more #includes of files that are temporarily\n'
        'allowed but being removed. Can you avoid introducing the\n'
        '#include? See relevant DEPS file(s) for details and contacts.',
        warning_descriptions))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.CheckOwners(
      input_api, output_api, source_file_filter=None))
  results.extend(input_api.canned_checks.CheckPatchFormatted(
      input_api, output_api))
  results.extend(_V8PresubmitChecks(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  return results


def _SkipTreeCheck(input_api, output_api):
  """Check the env var whether we want to skip tree check.
     Only skip if src/version.cc has been updated."""
  src_version = 'src/version.cc'
  FilterFile = lambda file: file.LocalPath() == src_version
  if not input_api.AffectedSourceFiles(
      lambda file: file.LocalPath() == src_version):
    return False
  return input_api.environ.get('PRESUBMIT_TREE_CHECK') == 'skip'


def _CheckChangeLogFlag(input_api, output_api):
  """Checks usage of LOG= flag in the commit message."""
  results = []
  if input_api.change.BUG and not 'LOG' in input_api.change.tags:
    results.append(output_api.PresubmitError(
        'An issue reference (BUG=) requires a change log flag (LOG=). '
        'Use LOG=Y for including this commit message in the change log. '
        'Use LOG=N or leave blank otherwise.'))
  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_CheckChangeLogFlag(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(_CheckChangeLogFlag(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  if not _SkipTreeCheck(input_api, output_api):
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://v8-status.appspot.com/current?format=json'))
  return results


def GetPreferredTryMasters(project, change):
  return {
    'tryserver.v8': {
      'v8_linux_rel': set(['defaulttests']),
      'v8_linux_dbg': set(['defaulttests']),
      'v8_linux_nosnap_rel': set(['defaulttests']),
      'v8_linux_nosnap_dbg': set(['defaulttests']),
      'v8_linux64_rel': set(['defaulttests']),
      'v8_linux_arm_dbg': set(['defaulttests']),
      'v8_linux_arm64_rel': set(['defaulttests']),
      'v8_linux_layout_dbg': set(['defaulttests']),
      'v8_mac_rel': set(['defaulttests']),
      'v8_win_rel': set(['defaulttests']),
      'v8_win64_rel': set(['defaulttests']),
    },
  }
