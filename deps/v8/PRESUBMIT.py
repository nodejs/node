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


_EXCLUDED_PATHS = (
    r"^test[\\\/].*",
    r"^testing[\\\/].*",
    r"^third_party[\\\/].*",
    r"^tools[\\\/].*",
)


# Regular expression that matches code only used for test binaries
# (best effort).
_TEST_CODE_EXCLUDED_PATHS = (
    r'.+-unittest\.cc',
    # Has a method VisitForTest().
    r'src[\\\/]compiler[\\\/]ast-graph-builder\.cc',
    # Test extension.
    r'src[\\\/]extensions[\\\/]gc-extension\.cc',
)


_TEST_ONLY_WARNING = (
    'You might be calling functions intended only for testing from\n'
    'production code.  It is OK to ignore this warning if you know what\n'
    'you are doing, as the heuristics used to detect the situation are\n'
    'not perfect.  The commit queue will not block on this warning.')


def _V8PresubmitChecks(input_api, output_api):
  """Runs the V8 presubmit checks."""
  import sys
  sys.path.append(input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools'))
  from presubmit import CppLintProcessor
  from presubmit import SourceProcessor
  from presubmit import CheckExternalReferenceRegistration
  from presubmit import CheckAuthorizedAuthor
  from presubmit import CheckStatusFiles

  results = []
  if not CppLintProcessor().Run(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError("C++ lint check failed"))
  if not SourceProcessor().Run(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError(
        "Copyright header, trailing whitespaces and two empty lines " \
        "between declarations check failed"))
  if not CheckExternalReferenceRegistration(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError(
        "External references registration check failed"))
  if not CheckStatusFiles(input_api.PresubmitLocalPath()):
    results.append(output_api.PresubmitError("Status file check failed"))
  results.extend(CheckAuthorizedAuthor(input_api, output_api))
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


def _CheckNoInlineHeaderIncludesInNormalHeaders(input_api, output_api):
  """Attempts to prevent inclusion of inline headers into normal header
  files. This tries to establish a layering where inline headers can be
  included by other inline headers or compilation units only."""
  file_inclusion_pattern = r'(?!.+-inl\.h).+\.h'
  include_directive_pattern = input_api.re.compile(r'#include ".+-inl.h"')
  include_warning = (
    'You might be including an inline header (e.g. foo-inl.h) within a\n'
    'normal header (e.g. bar.h) file.  Can you avoid introducing the\n'
    '#include?  The commit queue will not block on this warning.')

  def FilterFile(affected_file):
    black_list = (_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(
      affected_file,
      white_list=(file_inclusion_pattern, ),
      black_list=black_list)

  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    local_path = f.LocalPath()
    for line_number, line in f.ChangedContents():
      if (include_directive_pattern.search(line)):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(include_warning, problems)]
  else:
    return []


def _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api):
  """Attempts to prevent use of functions intended only for testing in
  non-testing code. For now this is just a best-effort implementation
  that ignores header files and may have some false positives. A
  better implementation would probably need a proper C++ parser.
  """
  # We only scan .cc files, as the declaration of for-testing functions in
  # header files are hard to distinguish from calls to such functions without a
  # proper C++ parser.
  file_inclusion_pattern = r'.+\.cc'

  base_function_pattern = r'[ :]test::[^\s]+|ForTest(ing)?|for_test(ing)?'
  inclusion_pattern = input_api.re.compile(r'(%s)\s*\(' % base_function_pattern)
  comment_pattern = input_api.re.compile(r'//.*(%s)' % base_function_pattern)
  exclusion_pattern = input_api.re.compile(
    r'::[A-Za-z0-9_]+(%s)|(%s)[^;]+\{' % (
      base_function_pattern, base_function_pattern))

  def FilterFile(affected_file):
    black_list = (_EXCLUDED_PATHS +
                  _TEST_CODE_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(
      affected_file,
      white_list=(file_inclusion_pattern, ),
      black_list=black_list)

  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    local_path = f.LocalPath()
    for line_number, line in f.ChangedContents():
      if (inclusion_pattern.search(line) and
          not comment_pattern.search(line) and
          not exclusion_pattern.search(line)):
        problems.append(
          '%s:%d\n    %s' % (local_path, line_number, line.strip()))

  if problems:
    return [output_api.PresubmitPromptOrNotify(_TEST_ONLY_WARNING, problems)]
  else:
    return []


def _CheckMissingFiles(input_api, output_api):
  """Runs verify_source_deps.py to ensure no files were added that are not in
  GN.
  """
  # We need to wait until we have an input_api object and use this
  # roundabout construct to import checkdeps because this file is
  # eval-ed and thus doesn't have __file__.
  original_sys_path = sys.path
  try:
    sys.path = sys.path + [input_api.os_path.join(
        input_api.PresubmitLocalPath(), 'tools')]
    from verify_source_deps import missing_gn_files, missing_gyp_files
  finally:
    # Restore sys.path to what it was before.
    sys.path = original_sys_path

  gn_files = missing_gn_files()
  gyp_files = missing_gyp_files()
  results = []
  if gn_files:
    results.append(output_api.PresubmitError(
        "You added one or more source files but didn't update the\n"
        "corresponding BUILD.gn files:\n",
        gn_files))
  if gyp_files:
    results.append(output_api.PresubmitError(
        "You added one or more source files but didn't update the\n"
        "corresponding gyp files:\n",
        gyp_files))
  return results


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(input_api.canned_checks.CheckOwners(
      input_api, output_api, source_file_filter=None))
  results.extend(input_api.canned_checks.CheckPatchFormatted(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckGenderNeutral(
      input_api, output_api))
  results.extend(_V8PresubmitChecks(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(
      _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api))
  results.extend(
      _CheckNoInlineHeaderIncludesInNormalHeaders(input_api, output_api))
  results.extend(_CheckMissingFiles(input_api, output_api))
  return results


def _SkipTreeCheck(input_api, output_api):
  """Check the env var whether we want to skip tree check.
     Only skip if include/v8-version.h has been updated."""
  src_version = 'include/v8-version.h'
  if not input_api.AffectedSourceFiles(
      lambda file: file.LocalPath() == src_version):
    return False
  return input_api.environ.get('PRESUBMIT_TREE_CHECK') == 'skip'


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(_CommonChecks(input_api, output_api))
  results.extend(input_api.canned_checks.CheckChangeHasDescription(
      input_api, output_api))
  if not _SkipTreeCheck(input_api, output_api):
    results.extend(input_api.canned_checks.CheckTreeIsOpen(
        input_api, output_api,
        json_url='http://v8-status.appspot.com/current?format=json'))
  return results
