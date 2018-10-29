#!/usr/bin/env python
#
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import multiprocessing
import optparse
import os
import re
import subprocess
import sys

CLANG_TIDY_WARNING = re.compile(r'(\/.*?)\ .*\[(.*)\]$')
CLANG_TIDY_CMDLINE_OUT = re.compile(r'^clang-tidy.*\ .*|^\./\.\*')
FILE_REGEXS = ['../src/*', '../test/*']
HEADER_REGEX = ['\.\.\/src\/.*|\.\.\/include\/.*|\.\.\/test\/.*']

THREADS = multiprocessing.cpu_count()


class ClangTidyWarning(object):
  """
  Wraps up a clang-tidy warning to present aggregated information.
  """

  def __init__(self, warning_type):
    self.warning_type = warning_type
    self.occurrences = set()

  def add_occurrence(self, file_path):
    self.occurrences.add(file_path.lstrip())

  def __hash__(self):
    return hash(self.warning_type)

  def to_string(self, file_loc):
    s = '[%s] #%d\n' % (self.warning_type, len(self.occurrences))
    if file_loc:
      s += ' ' + '\n  '.join(self.occurrences)
      s += '\n'
    return s

  def __str__(self):
    return self.to_string(False)

  def __lt__(self, other):
    return len(self.occurrences) < len(other.occurrences)


def GenerateCompileCommands(build_folder):
  """
  Generate a compilation database.

  Currently clang-tidy-4 does not understand all flags that are passed
  by the build system, therefore, we remove them from the generated file.
  """
  ninja_ps = subprocess.Popen(
    ['ninja', '-t', 'compdb', 'cxx', 'cc'],
    stdout=subprocess.PIPE,
    cwd=build_folder)

  out_filepath = os.path.join(build_folder, 'compile_commands.json')
  with open(out_filepath, 'w') as cc_file:
    while True:
        line = ninja_ps.stdout.readline()

        if line == '':
            break

        line = line.replace('-fcomplete-member-pointers', '')
        line = line.replace('-Wno-enum-compare-switch', '')
        line = line.replace('-Wno-ignored-pragma-optimize', '')
        line = line.replace('-Wno-null-pointer-arithmetic', '')
        line = line.replace('-Wno-unused-lambda-capture', '')
        line = line.replace('-Wno-defaulted-function-deleted', '')
        cc_file.write(line)


def skip_line(line):
  """
  Check if a clang-tidy output line should be skipped.
  """
  return bool(CLANG_TIDY_CMDLINE_OUT.search(line))


def ClangTidyRunFull(build_folder, skip_output_filter, checks, auto_fix):
  """
  Run clang-tidy on the full codebase and print warnings.
  """
  extra_args = []
  if auto_fix:
    extra_args.append('-fix')

  if checks is not None:
    extra_args.append('-checks')
    extra_args.append('-*, ' + checks)

  with open(os.devnull, 'w') as DEVNULL:
    ct_process = subprocess.Popen(
      ['run-clang-tidy', '-j' + str(THREADS), '-p', '.']
       + ['-header-filter'] + HEADER_REGEX + extra_args
       + FILE_REGEXS,
      cwd=build_folder,
      stdout=subprocess.PIPE,
      stderr=DEVNULL)
  removing_check_header = False
  empty_lines = 0

  while True:
    line = ct_process.stdout.readline()
    if line == '':
      break

    # Skip all lines after Enbale checks and before two newlines,
    # i.e., skip clang-tidy check list.
    if line.startswith('Enabled checks'):
      removing_check_header = True
    if removing_check_header and not skip_output_filter:
      if line == '\n':
        empty_lines += 1
      if empty_lines == 2:
        removing_check_header = False
      continue

    # Different lines get removed to ease output reading.
    if not skip_output_filter and skip_line(line):
      continue

    # Print line, because no filter was matched.
    if line != '\n':
        sys.stdout.write(line)


def ClangTidyRunAggregate(build_folder, print_files):
  """
  Run clang-tidy on the full codebase and aggregate warnings into categories.
  """
  with open(os.devnull, 'w') as DEVNULL:
    ct_process = subprocess.Popen(
      ['run-clang-tidy', '-j' + str(THREADS), '-p', '.'] +
        ['-header-filter'] + HEADER_REGEX +
        FILE_REGEXS,
      cwd=build_folder,
      stdout=subprocess.PIPE,
      stderr=DEVNULL)
  warnings = dict()
  while True:
    line = ct_process.stdout.readline()
    if line == '':
      break

    res = CLANG_TIDY_WARNING.search(line)
    if res is not None:
      warnings.setdefault(
          res.group(2),
          ClangTidyWarning(res.group(2))).add_occurrence(res.group(1))

  for warning in sorted(warnings.values(), reverse=True):
    sys.stdout.write(warning.to_string(print_files))


def ClangTidyRunDiff(build_folder, diff_branch, auto_fix):
  """
  Run clang-tidy on the diff between current and the diff_branch.
  """
  if diff_branch is None:
    diff_branch = subprocess.check_output(['git', 'merge-base',
                                           'HEAD', 'origin/master']).strip()

  git_ps = subprocess.Popen(
    ['git', 'diff', '-U0', diff_branch], stdout=subprocess.PIPE)

  extra_args = []
  if auto_fix:
    extra_args.append('-fix')

  with open(os.devnull, 'w') as DEVNULL:
    """
    The script `clang-tidy-diff` does not provide support to add header-
    filters. To still analyze headers we use the build path option `-path` to
    inject our header-filter option. This works because the script just adds
    the passed path string to the commandline of clang-tidy.
    """
    modified_build_folder = build_folder
    modified_build_folder += ' -header-filter='
    modified_build_folder += '\'' + ''.join(HEADER_REGEX) + '\''

    ct_ps = subprocess.Popen(
      ['clang-tidy-diff.py', '-path', modified_build_folder, '-p1'] +
        extra_args,
      stdin=git_ps.stdout,
      stdout=subprocess.PIPE,
      stderr=DEVNULL)
  git_ps.wait()
  while True:
    line = ct_ps.stdout.readline()
    if line == '':
      break

    if skip_line(line):
      continue

    sys.stdout.write(line)


def rm_prefix(string, prefix):
  """
  Removes prefix from a string until the new string
  no longer starts with the prefix.
  """
  while string.startswith(prefix):
    string = string[len(prefix):]
  return string


def ClangTidyRunSingleFile(build_folder, filename_to_check, auto_fix,
                           line_ranges=[]):
  """
  Run clang-tidy on a single file.
  """
  files_with_relative_path = []

  compdb_filepath = os.path.join(build_folder, 'compile_commands.json')
  with open(compdb_filepath) as raw_json_file:
    compdb = json.load(raw_json_file)

  for db_entry in compdb:
    if db_entry['file'].endswith(filename_to_check):
      files_with_relative_path.append(db_entry['file'])

  with open(os.devnull, 'w') as DEVNULL:
    for file_with_relative_path in files_with_relative_path:
      line_filter = None
      if len(line_ranges) != 0:
        line_filter = '['
        line_filter += '{ \"lines\":[' + ', '.join(line_ranges)
        line_filter += '], \"name\":\"'
        line_filter += rm_prefix(file_with_relative_path,
                                 '../') + '\"}'
        line_filter += ']'

      extra_args = ['-line-filter=' + line_filter] if line_filter else []

      if auto_fix:
        extra_args.append('-fix')

      subprocess.call(['clang-tidy', '-p', '.'] +
                      extra_args +
                      [file_with_relative_path],
                      cwd=build_folder,
                      stderr=DEVNULL)


def CheckClangTidy():
  """
  Checks if a clang-tidy binary exists.
  """
  with open(os.devnull, 'w') as DEVNULL:
    return subprocess.call(['which', 'clang-tidy'], stdout=DEVNULL) == 0


def CheckCompDB(build_folder):
  """
  Checks if a compilation database exists in the build_folder.
  """
  return os.path.isfile(os.path.join(build_folder, 'compile_commands.json'))


def DetectBuildFolder():
    """
    Tries to auto detect the last used build folder in out/
    """
    outdirs_folder = 'out/'
    last_used = None
    last_timestamp = -1
    for outdir in [outdirs_folder + folder_name
                   for folder_name in os.listdir(outdirs_folder)
                   if os.path.isdir(outdirs_folder + folder_name)]:
        outdir_modified_timestamp = os.path.getmtime(outdir)
        if  outdir_modified_timestamp > last_timestamp:
            last_timestamp = outdir_modified_timestamp
            last_used = outdir

    return last_used


def GetOptions():
  """
  Generate the option parser for this script.
  """
  result = optparse.OptionParser()
  result.add_option(
    '-b',
    '--build-folder',
    help='Set V8 build folder',
    dest='build_folder',
    default=None)
  result.add_option(
    '-j',
    help='Set the amount of threads that should be used',
    dest='threads',
    default=None)
  result.add_option(
    '--gen-compdb',
    help='Generate a compilation database for clang-tidy',
    default=False,
    action='store_true')
  result.add_option(
    '--no-output-filter',
    help='Done use any output filterning',
    default=False,
    action='store_true')
  result.add_option(
    '--fix',
    help='Fix auto fixable issues',
    default=False,
    dest='auto_fix',
    action='store_true'
  )

  # Full clang-tidy.
  full_run_g = optparse.OptionGroup(result, 'Clang-tidy full', '')
  full_run_g.add_option(
    '--full',
    help='Run clang-tidy on the whole codebase',
    default=False,
    action='store_true')
  full_run_g.add_option('--checks',
                        help='Clang-tidy checks to use.',
                        default=None)
  result.add_option_group(full_run_g)

  # Aggregate clang-tidy.
  agg_run_g = optparse.OptionGroup(result, 'Clang-tidy aggregate', '')
  agg_run_g.add_option('--aggregate', help='Run clang-tidy on the whole '\
             'codebase and aggregate the warnings',
             default=False, action='store_true')
  agg_run_g.add_option('--show-loc', help='Show file locations when running '\
             'in aggregate mode', default=False,
             action='store_true')
  result.add_option_group(agg_run_g)

  # Diff clang-tidy.
  diff_run_g = optparse.OptionGroup(result, 'Clang-tidy diff', '')
  diff_run_g.add_option('--branch', help='Run clang-tidy on the diff '\
             'between HEAD and the merge-base between HEAD '\
             'and DIFF_BRANCH (origin/master by default).',
             default=None, dest='diff_branch')
  result.add_option_group(diff_run_g)

  # Single clang-tidy.
  single_run_g = optparse.OptionGroup(result, 'Clang-tidy single', '')
  single_run_g.add_option(
    '--single', help='', default=False, action='store_true')
  single_run_g.add_option(
    '--file', help='File name to check', default=None, dest='file_name')
  single_run_g.add_option('--lines', help='Limit checks to a line range. '\
              'For example: --lines="[2,4], [5,6]"',
              default=[], dest='line_ranges')

  result.add_option_group(single_run_g)
  return result


def main():
  parser = GetOptions()
  (options, _) = parser.parse_args()

  if options.threads is not None:
    global THREADS
    THREADS = options.threads

  if options.build_folder is None:
    options.build_folder = DetectBuildFolder()

  if not CheckClangTidy():
    print 'Could not find clang-tidy'
  elif options.build_folder is None or not os.path.isdir(options.build_folder):
    print 'Please provide a build folder with -b'
  elif options.gen_compdb:
    GenerateCompileCommands(options.build_folder)
  elif not CheckCompDB(options.build_folder):
    print 'Could not find compilation database, ' \
      'please generate it with --gen-compdb'
  else:
    print 'Using build folder:', options.build_folder
    if options.full:
      print 'Running clang-tidy - full'
      ClangTidyRunFull(options.build_folder,
                       options.no_output_filter,
                       options.checks,
                       options.auto_fix)
    elif options.aggregate:
      print 'Running clang-tidy - aggregating warnings'
      if options.auto_fix:
        print 'Auto fix not working in aggregate mode, running without.'
      ClangTidyRunAggregate(options.build_folder, options.show_loc)
    elif options.single:
      print 'Running clang-tidy - single on ' + options.file_name
      if options.file_name is not None:
        line_ranges = []
        for match in re.findall(r'(\[.*?\])', options.line_ranges):
          if match is not []:
            line_ranges.append(match)
        ClangTidyRunSingleFile(options.build_folder,
                               options.file_name,
                               options.auto_fix,
                               line_ranges)
      else:
        print 'Filename provided, please specify a filename with --file'
    else:
      print 'Running clang-tidy'
      ClangTidyRunDiff(options.build_folder,
                       options.diff_branch,
                       options.auto_fix)


if __name__ == '__main__':
  main()
