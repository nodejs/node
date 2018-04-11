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

import json
import re
import sys


_EXCLUDED_PATHS = (
    r"^test[\\\/].*",
    r"^testing[\\\/].*",
    r"^third_party[\\\/].*",
    r"^tools[\\\/].*",
)


# Regular expression that matches code which should not be run through cpplint.
_NO_LINT_PATHS = (
    r'src[\\\/]base[\\\/]export-template\.h',
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
  from presubmit import StatusFilesProcessor

  def FilterFile(affected_file):
    return input_api.FilterSourceFile(
      affected_file,
      white_list=None,
      black_list=_NO_LINT_PATHS)

  results = []
  if not CppLintProcessor().RunOnFiles(
      input_api.AffectedFiles(file_filter=FilterFile, include_deletes=False)):
    results.append(output_api.PresubmitError("C++ lint check failed"))
  if not SourceProcessor().RunOnFiles(
      input_api.AffectedFiles(include_deletes=False)):
    results.append(output_api.PresubmitError(
        "Copyright header, trailing whitespaces and two empty lines " \
        "between declarations check failed"))
  if not StatusFilesProcessor().RunOnFiles(
      input_api.AffectedFiles(include_deletes=True)):
    results.append(output_api.PresubmitError("Status file check failed"))
  results.extend(input_api.canned_checks.CheckAuthorizedAuthor(
      input_api, output_api))
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


def _CheckHeadersHaveIncludeGuards(input_api, output_api):
  """Ensures that all header files have include guards."""
  file_inclusion_pattern = r'src/.+\.h'

  def FilterFile(affected_file):
    black_list = (_EXCLUDED_PATHS +
                  input_api.DEFAULT_BLACK_LIST)
    return input_api.FilterSourceFile(
      affected_file,
      white_list=(file_inclusion_pattern, ),
      black_list=black_list)

  leading_src_pattern = input_api.re.compile(r'^src/')
  dash_dot_slash_pattern = input_api.re.compile(r'[-./]')
  def PathToGuardMacro(path):
    """Guards should be of the form V8_PATH_TO_FILE_WITHOUT_SRC_H_."""
    x = input_api.re.sub(leading_src_pattern, 'v8_', path)
    x = input_api.re.sub(dash_dot_slash_pattern, '_', x)
    x = x.upper() + "_"
    return x

  problems = []
  for f in input_api.AffectedSourceFiles(FilterFile):
    local_path = f.LocalPath()
    guard_macro = PathToGuardMacro(local_path)
    guard_patterns = [
            input_api.re.compile(r'^#ifndef ' + guard_macro + '$'),
            input_api.re.compile(r'^#define ' + guard_macro + '$'),
            input_api.re.compile(r'^#endif  // ' + guard_macro + '$')]
    skip_check_pattern = input_api.re.compile(
            r'^// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD')
    found_patterns = [ False, False, False ]
    file_omitted = False

    for line in f.NewContents():
      for i in range(len(guard_patterns)):
        if guard_patterns[i].match(line):
            found_patterns[i] = True
      if skip_check_pattern.match(line):
        file_omitted = True
        break

    if not file_omitted and not all(found_patterns):
      problems.append(
        '%s: Missing include guard \'%s\'' % (local_path, guard_macro))

  if problems:
    return [output_api.PresubmitError(
        'You added one or more header files without an appropriate\n'
        'include guard. Add the include guard {#ifndef,#define,#endif}\n'
        'triplet or omit the check entirely through the magic comment:\n'
        '"// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD".', problems)]
  else:
    return []


# TODO(mstarzinger): Similar checking should be made available as part of
# tools/presubmit.py (note that tools/check-inline-includes.sh exists).
def _CheckNoInlineHeaderIncludesInNormalHeaders(input_api, output_api):
  """Attempts to prevent inclusion of inline headers into normal header
  files. This tries to establish a layering where inline headers can be
  included by other inline headers or compilation units only."""
  file_inclusion_pattern = r'(?!.+-inl\.h).+\.h'
  include_directive_pattern = input_api.re.compile(r'#include ".+-inl.h"')
  include_error = (
    'You are including an inline header (e.g. foo-inl.h) within a normal\n'
    'header (e.g. bar.h) file.  This violates layering of dependencies.')

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
    return [output_api.PresubmitError(include_error, problems)]
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


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  results = []
  results.extend(_CheckCommitMessageBugEntry(input_api, output_api))
  results.extend(input_api.canned_checks.CheckPatchFormatted(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckGenderNeutral(
      input_api, output_api))
  results.extend(_V8PresubmitChecks(input_api, output_api))
  results.extend(_CheckUnwantedDependencies(input_api, output_api))
  results.extend(
      _CheckNoProductionCodeUsingTestOnlyFunctions(input_api, output_api))
  results.extend(_CheckHeadersHaveIncludeGuards(input_api, output_api))
  results.extend(
      _CheckNoInlineHeaderIncludesInNormalHeaders(input_api, output_api))
  results.extend(_CheckJSONFiles(input_api, output_api))
  results.extend(_CheckMacroUndefs(input_api, output_api))
  results.extend(input_api.RunTests(
    input_api.canned_checks.CheckVPythonSpec(input_api, output_api)))
  return results


def _SkipTreeCheck(input_api, output_api):
  """Check the env var whether we want to skip tree check.
     Only skip if include/v8-version.h has been updated."""
  src_version = 'include/v8-version.h'
  if not input_api.AffectedSourceFiles(
      lambda file: file.LocalPath() == src_version):
    return False
  return input_api.environ.get('PRESUBMIT_TREE_CHECK') == 'skip'


def _CheckCommitMessageBugEntry(input_api, output_api):
  """Check that bug entries are well-formed in commit message."""
  bogus_bug_msg = (
      'Bogus BUG entry: %s. Please specify the issue tracker prefix and the '
      'issue number, separated by a colon, e.g. v8:123 or chromium:12345.')
  results = []
  for bug in (input_api.change.BUG or '').split(','):
    bug = bug.strip()
    if 'none'.startswith(bug.lower()):
      continue
    if ':' not in bug:
      try:
        if int(bug) > 100000:
          # Rough indicator for current chromium bugs.
          prefix_guess = 'chromium'
        else:
          prefix_guess = 'v8'
        results.append('BUG entry requires issue tracker prefix, e.g. %s:%s' %
                       (prefix_guess, bug))
      except ValueError:
        results.append(bogus_bug_msg % bug)
    elif not re.match(r'\w+:\d+', bug):
      results.append(bogus_bug_msg % bug)
  return [output_api.PresubmitError(r) for r in results]


def _CheckJSONFiles(input_api, output_api):
  def FilterFile(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        white_list=(r'.+\.json',))

  results = []
  for f in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    with open(f.LocalPath()) as j:
      try:
        json.load(j)
      except Exception as e:
        results.append(
            'JSON validation failed for %s. Error:\n%s' % (f.LocalPath(), e))

  return [output_api.PresubmitError(r) for r in results]


def _CheckMacroUndefs(input_api, output_api):
  """
  Checks that each #define in a .cc file is eventually followed by an #undef.

  TODO(clemensh): This check should eventually be enabled for all cc files via
  tools/presubmit.py (https://crbug.com/v8/6811).
  """
  def FilterFile(affected_file):
    # Skip header files, as they often define type lists which are used in
    # other files.
    white_list = (r'.+\.cc',r'.+\.cpp',r'.+\.c')
    return input_api.FilterSourceFile(affected_file, white_list=white_list)

  def TouchesMacros(f):
    for line in f.GenerateScmDiff().splitlines():
      if not line.startswith('+') and not line.startswith('-'):
        continue
      if define_pattern.match(line[1:]) or undef_pattern.match(line[1:]):
        return True
    return False

  define_pattern = input_api.re.compile(r'#define (\w+)')
  undef_pattern = input_api.re.compile(r'#undef (\w+)')
  errors = []
  for f in input_api.AffectedFiles(
      file_filter=FilterFile, include_deletes=False):
    if not TouchesMacros(f):
      continue

    defined_macros = dict()
    with open(f.LocalPath()) as fh:
      line_nr = 0
      for line in fh:
        line_nr += 1

        define_match = define_pattern.match(line)
        if define_match:
          name = define_match.group(1)
          defined_macros[name] = line_nr

        undef_match = undef_pattern.match(line)
        if undef_match:
          name = undef_match.group(1)
          if not name in defined_macros:
            errors.append('{}:{}: Macro named \'{}\' was not defined before.'
                          .format(f.LocalPath(), line_nr, name))
          else:
            del defined_macros[name]
    for name, line_nr in sorted(defined_macros.items(), key=lambda e: e[1]):
      errors.append('{}:{}: Macro missing #undef: {}'
                    .format(f.LocalPath(), line_nr, name))

  if errors:
    return [output_api.PresubmitPromptOrNotify(
        'Detected mismatches in #define / #undef in the file(s) where you '
        'modified preprocessor macros.',
        errors)]
  return []


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

def PostUploadHook(cl, change, output_api):
  """git cl upload will call this hook after the issue is created/modified.

  This hook adds a noi18n bot if the patch affects Intl.
  """
  def affects_intl(f):
    return 'intl' in f.LocalPath() or 'test262' in f.LocalPath()
  if not change.AffectedFiles(file_filter=affects_intl):
    return []
  return output_api.EnsureCQIncludeTrybotsAreAdded(
      cl,
      [
        'luci.v8.try:v8_linux_noi18n_rel_ng'
      ],
      'Automatically added noi18n trybots to run tests on CQ.')
