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

import itertools
import subprocess


def CheckChange(input, output):
  # There apparently is no way to wrap strings in blueprints, so ignore long
  # lines in them.
  def long_line_sources(x):
    return input.FilterSourceFile(
        x,
        white_list=".*",
        black_list=[
            'Android[.]bp', '.*[.]json$', '.*[.]sql$', '.*[.]out$',
            'test/trace_processor/index$', '.*\bBUILD$', 'WORKSPACE',
            '.*/Makefile$', '/perfetto_build_flags.h$'
        ])

  results = []
  results += input.canned_checks.CheckDoNotSubmit(input, output)
  results += input.canned_checks.CheckChangeHasNoTabs(input, output)
  results += input.canned_checks.CheckLongLines(
      input, output, 80, source_file_filter=long_line_sources)
  results += input.canned_checks.CheckPatchFormatted(
      input, output, check_js=True)
  results += input.canned_checks.CheckGNFormatted(input, output)
  results += CheckIncludeGuards(input, output)
  results += CheckIncludeViolations(input, output)
  results += CheckBuild(input, output)
  results += CheckAndroidBlueprint(input, output)
  results += CheckBinaryDescriptors(input, output)
  results += CheckMergedTraceConfigProto(input, output)
  results += CheckWhitelist(input, output)
  results += CheckBannedCpp(input, output)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckBuild(input_api, output_api):
  tool = 'tools/gen_bazel'

  # If no GN files were modified, bail out.
  def build_file_filter(x):
    return input_api.FilterSourceFile(
        x, white_list=('.*BUILD[.]gn$', '.*[.]gni$', 'BUILD\.extras', tool))

  if not input_api.AffectedSourceFiles(build_file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError('Bazel BUILD(s) are out of date. Run ' +
                                  tool + ' to update them.')
    ]
  return []


def CheckAndroidBlueprint(input_api, output_api):
  tool = 'tools/gen_android_bp'

  # If no GN files were modified, bail out.
  def build_file_filter(x):
    return input_api.FilterSourceFile(
        x, white_list=('.*BUILD[.]gn$', '.*[.]gni$', tool))

  if not input_api.AffectedSourceFiles(build_file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError('Android build files are out of date. ' +
                                  'Run ' + tool + ' to update them.')
    ]
  return []


def CheckIncludeGuards(input_api, output_api):
  tool = 'tools/fix_include_guards'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, white_list=['.*[.]cc$', '.*[.]h$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError('Please run ' + tool +
                                  ' to fix include guards.')
    ]
  return []


def CheckBannedCpp(input_api, output_api):
  bad_cpp = [
      (r'\bNULL\b', 'New code should not use NULL prefer nullptr'),
      (r'\bstd::stoi\b',
       'std::stoi throws exceptions prefer base::StringToInt32()'),
      (r'\bstd::stol\b',
       'std::stoull throws exceptions prefer base::StringToInt32()'),
      (r'\bstd::stoul\b',
       'std::stoull throws exceptions prefer base::StringToUint32()'),
      (r'\bstd::stoll\b',
       'std::stoull throws exceptions prefer base::StringToInt64()'),
      (r'\bstd::stoull\b',
       'std::stoull throws exceptions prefer base::StringToUint64()'),
      (r'\bstd::stof\b',
       'std::stof throws exceptions prefer base::StringToDouble()'),
      (r'\bstd::stod\b',
       'std::stod throws exceptions prefer base::StringToDouble()'),
      (r'\bstd::stold\b',
       'std::stold throws exceptions prefer base::StringToDouble()'),
      (r'\bPERFETTO_EINTR\(close\(',
       'close(2) must not be retried on EINTR on Linux and other OSes '
       'that we run on, as the fd will be closed.'),
  ]

  def file_filter(x):
    return input_api.FilterSourceFile(x, white_list=[r'.*\.h$', r'.*\.cc$'])

  errors = []
  for f in input_api.AffectedSourceFiles(file_filter):
    for line_number, line in f.ChangedContents():
      for regex, message in bad_cpp:
        if input_api.re.search(regex, line):
          errors.append(
              output_api.PresubmitError('Banned pattern:\n  {}:{} {}'.format(
                  f.LocalPath(), line_number, message)))
  return errors


def CheckIncludeViolations(input_api, output_api):
  tool = 'tools/check_include_violations'

  def file_filter(x):
    return input_api.FilterSourceFile(x, white_list=['include/.*[.]h$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool]):
    return [output_api.PresubmitError(tool + ' failed.')]
  return []


def CheckBinaryDescriptors(input_api, output_api):
  tool = 'tools/gen_binary_descriptors'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, white_list=['protos/perfetto/.*[.]proto$', '.*[.]h', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError('Please run ' + tool +
                                  ' to update binary descriptors.')
    ]
  return []


def CheckMergedTraceConfigProto(input_api, output_api):
  tool = 'tools/gen_merged_protos'

  def build_file_filter(x):
    return input_api.FilterSourceFile(
        x, white_list=['protos/perfetto/.*[.]proto$', tool])

  if not input_api.AffectedSourceFiles(build_file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError(
            'perfetto_config.proto or perfetto_trace.proto is out of ' +
            'date. Please run ' + tool + ' to update it.')
    ]
  return []


# Prevent removing or changing lines in event_whitelist.
def CheckWhitelist(input_api, output_api):
  for f in input_api.AffectedFiles():
    if f.LocalPath() != 'tools/ftrace_proto_gen/event_whitelist':
      continue
    if any((not new_line.startswith('removed')) and new_line != old_line
           for old_line, new_line in itertools.izip(f.OldContents(),
                                                    f.NewContents())):
      return [
          output_api.PresubmitError(
              'event_whitelist only has two supported changes: '
              'appending a new line, and replacing a line with removed.')
      ]
  return []
