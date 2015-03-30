#!/usr/bin/env python
# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for analyzer
"""

import json
import TestGyp

found = 'Found dependency'
found_all = 'Found dependency (all)'
not_found = 'No dependencies'


def _CreateConfigFile(files, targets):
  """Creates the analyzer conflig file, which is used as the input to analyzer.
  See description of analyzer.py for description of the arguments."""
  f = open('test_file', 'w')
  to_write = {'files': files, 'targets': targets }
  json.dump(to_write, f)
  f.close()


def _CreateBogusConfigFile():
  f = open('test_file','w')
  f.write('bogus')
  f.close()


def _ReadOutputFileContents():
  f = open('analyzer_output', 'r')
  result = json.load(f)
  f.close()
  return result


# NOTE: this would be clearer if it subclassed TestGypCustom, but that trips
# over a bug in pylint (E1002).
test = TestGyp.TestGypCustom(format='analyzer')

def CommonArgs():
  return ('-Gconfig_path=test_file',
           '-Ganalyzer_output_path=analyzer_output')


def run_analyzer(*args, **kw):
  """Runs the test specifying a particular config and output path."""
  args += CommonArgs()
  test.run_gyp('test.gyp', *args, **kw)


def run_analyzer2(*args, **kw):
  """Same as run_analyzer(), but passes in test2.gyp instead of test.gyp."""
  args += CommonArgs()
  test.run_gyp('test2.gyp', *args, **kw)


def run_analyzer3(*args, **kw):
  """Same as run_analyzer(), but passes in test3.gyp instead of test.gyp."""
  args += CommonArgs()
  test.run_gyp('test3.gyp', *args, **kw)


def run_analyzer4(*args, **kw):
  """Same as run_analyzer(), but passes in test3.gyp instead of test.gyp."""
  args += CommonArgs()
  test.run_gyp('test4.gyp', *args, **kw)


def EnsureContains(targets=set(), matched=False, build_targets=set()):
  """Verifies output contains |targets|."""
  result = _ReadOutputFileContents()
  if result.get('error', None):
    print 'unexpected error', result.get('error')
    test.fail_test()

  if result.get('invalid_targets', None):
    print 'unexpected invalid_targets', result.get('invalid_targets')
    test.fail_test()

  actual_targets = set(result['targets'])
  if actual_targets != targets:
    print 'actual targets:', actual_targets, '\nexpected targets:', targets
    test.fail_test()

  actual_build_targets = set(result['build_targets'])
  if actual_build_targets != build_targets:
    print 'actual build_targets:', actual_build_targets, \
           '\nexpected build_targets:', build_targets
    test.fail_test()

  if matched and result['status'] != found:
    print 'expected', found, 'got', result['status']
    test.fail_test()
  elif not matched and result['status'] != not_found:
    print 'expected', not_found, 'got', result['status']
    test.fail_test()


def EnsureMatchedAll(targets):
  result = _ReadOutputFileContents()
  if result.get('error', None):
    print 'unexpected error', result.get('error')
    test.fail_test()

  if result.get('invalid_targets', None):
    print 'unexpected invalid_targets', result.get('invalid_targets')
    test.fail_test()

  if result['status'] != found_all:
    print 'expected', found_all, 'got', result['status']
    test.fail_test()

  actual_targets = set(result['targets'])
  if actual_targets != targets:
    print 'actual targets:', actual_targets, '\nexpected targets:', targets
    test.fail_test()


def EnsureError(expected_error_string):
  """Verifies output contains the error string."""
  result = _ReadOutputFileContents()
  if result.get('error', '').find(expected_error_string) == -1:
    print 'actual error:', result.get('error', ''), '\nexpected error:', \
        expected_error_string
    test.fail_test()


def EnsureStdoutContains(expected_error_string):
  if test.stdout().find(expected_error_string) == -1:
    print 'actual stdout:', test.stdout(), '\nexpected stdout:', \
        expected_error_string
    test.fail_test()


def EnsureInvalidTargets(expected_invalid_targets):
  """Verifies output contains invalid_targets."""
  result = _ReadOutputFileContents()
  actual_invalid_targets = set(result['invalid_targets'])
  if actual_invalid_targets != expected_invalid_targets:
    print 'actual invalid_targets:', actual_invalid_targets, \
        '\nexpected :', expected_invalid_targets
    test.fail_test()

# Verifies config_path must be specified.
test.run_gyp('test.gyp')
EnsureStdoutContains('Must specify files to analyze via config_path')

# Verifies config_path must point to a valid file.
test.run_gyp('test.gyp', '-Gconfig_path=bogus_file',
             '-Ganalyzer_output_path=analyzer_output')
EnsureError('Unable to open file bogus_file')

# Verify 'invalid_targets' is present when bad target is specified.
_CreateConfigFile(['exe2.c'], ['bad_target'])
run_analyzer()
EnsureInvalidTargets({'bad_target'})

# Verifies config_path must point to a valid json file.
_CreateBogusConfigFile()
run_analyzer()
EnsureError('Unable to parse config file test_file')

# Trivial test of a source.
_CreateConfigFile(['foo.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Conditional source that is excluded.
_CreateConfigFile(['conditional_source.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Conditional source that is included by way of argument.
_CreateConfigFile(['conditional_source.c'], [])
run_analyzer('-Dtest_variable=1')
EnsureContains(matched=True, build_targets={'exe'})

# Two unknown files.
_CreateConfigFile(['unknown1.c', 'unoknow2.cc'], [])
run_analyzer()
EnsureContains()

# Two unknown files.
_CreateConfigFile(['unknown1.c', 'subdir/subdir_sourcex.c'], [])
run_analyzer()
EnsureContains()

# Included dependency
_CreateConfigFile(['unknown1.c', 'subdir/subdir_source.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe', 'exe3'})

# Included inputs to actions.
_CreateConfigFile(['action_input.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Don't consider outputs.
_CreateConfigFile(['action_output.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Rule inputs.
_CreateConfigFile(['rule_input.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Ignore path specified with PRODUCT_DIR.
_CreateConfigFile(['product_dir_input.c'], [])
run_analyzer()
EnsureContains(matched=False)

# Path specified via a variable.
_CreateConfigFile(['subdir/subdir_source2.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Verifies paths with // are fixed up correctly.
_CreateConfigFile(['parent_source.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe', 'exe3'})

# Verifies relative paths are resolved correctly.
_CreateConfigFile(['subdir/subdir_source.h'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Various permutations when passing in targets.
_CreateConfigFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe', 'exe3'])
run_analyzer()
EnsureContains(matched=True, targets={'exe3'}, build_targets={'exe2', 'exe3'})

_CreateConfigFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe'])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2', 'exe3'})

# Verifies duplicates are ignored.
_CreateConfigFile(['exe2.c', 'subdir/subdir2b_source.c'], ['exe', 'exe'])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2', 'exe3'})

_CreateConfigFile(['exe2.c'], ['exe'])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2'})

_CreateConfigFile(['exe2.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2'})

_CreateConfigFile(['subdir/subdir2b_source.c', 'exe2.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2', 'exe3'})

_CreateConfigFile(['subdir/subdir2b_source.c'], ['exe3'])
run_analyzer()
EnsureContains(matched=True, targets={'exe3'}, build_targets={'exe3'})

_CreateConfigFile(['exe2.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe2'})

_CreateConfigFile(['foo.c'], [])
run_analyzer()
EnsureContains(matched=True, build_targets={'exe'})

# Assertions when modifying build (gyp/gypi) files, especially when said files
# are included.
_CreateConfigFile(['subdir2/d.cc'], ['exe', 'exe2', 'foo', 'exe3'])
run_analyzer2()
EnsureContains(matched=True, targets={'exe', 'foo'}, build_targets={'exe'})

_CreateConfigFile(['subdir2/subdir.includes.gypi'],
                ['exe', 'exe2', 'foo', 'exe3'])
run_analyzer2()
EnsureContains(matched=True, targets={'exe', 'foo'}, build_targets={'exe'})

_CreateConfigFile(['subdir2/subdir.gyp'], ['exe', 'exe2', 'foo', 'exe3'])
run_analyzer2()
EnsureContains(matched=True, targets={'exe', 'foo'}, build_targets={'exe'})

_CreateConfigFile(['test2.includes.gypi'], ['exe', 'exe2', 'foo', 'exe3'])
run_analyzer2()
EnsureContains(matched=True, targets={'exe', 'exe2', 'exe3'},
               build_targets={'exe', 'exe2', 'exe3'})

# Verify modifying a file included makes all targets dirty.
_CreateConfigFile(['common.gypi'], ['exe', 'exe2', 'foo', 'exe3'])
run_analyzer2('-Icommon.gypi')
EnsureMatchedAll({'exe', 'exe2', 'foo', 'exe3'})

# Assertions from test3.gyp.
_CreateConfigFile(['d.c', 'f.c'], ['a'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a', 'b'})

_CreateConfigFile(['f.c'], ['a'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a', 'b'})

_CreateConfigFile(['f.c'], [])
run_analyzer3()
EnsureContains(matched=True, build_targets={'a', 'b'})

_CreateConfigFile(['c.c', 'e.c'], [])
run_analyzer3()
EnsureContains(matched=True, build_targets={'a', 'b', 'c', 'e'})

_CreateConfigFile(['d.c'], ['a'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a', 'b'})

_CreateConfigFile(['a.c'], ['a', 'b'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a'})

_CreateConfigFile(['a.c'], ['a', 'b'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a'})

_CreateConfigFile(['d.c'], ['a', 'b'])
run_analyzer3()
EnsureContains(matched=True, targets={'a', 'b'}, build_targets={'a', 'b'})

_CreateConfigFile(['f.c'], ['a'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a', 'b'})

_CreateConfigFile(['a.c'], ['a'])
run_analyzer3()
EnsureContains(matched=True, targets={'a'}, build_targets={'a'})

_CreateConfigFile(['a.c'], [])
run_analyzer3()
EnsureContains(matched=True, build_targets={'a'})

_CreateConfigFile(['d.c'], [])
run_analyzer3()
EnsureContains(matched=True, build_targets={'a', 'b'})

# Assertions around test4.gyp.
_CreateConfigFile(['f.c'], [])
run_analyzer4()
EnsureContains(matched=True, build_targets={'e', 'f'})

_CreateConfigFile(['d.c'], [])
run_analyzer4()
EnsureContains(matched=True, build_targets={'a', 'b', 'c', 'd'})

_CreateConfigFile(['i.c'], [])
run_analyzer4()
EnsureContains(matched=True, build_targets={'h'})

test.pass_test()
