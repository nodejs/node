#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
V8 correctness fuzzer launcher script.
"""

import argparse
import hashlib
import itertools
import json
import os
import random
import re
import sys
import traceback

import v8_commands
import v8_suppressions

CONFIGS = dict(
  default=[
    '--suppress-asm-messages',
  ],
  ignition=[
    '--turbo-filter=~',
    '--noopt',
    '--suppress-asm-messages',
  ],
  ignition_asm=[
    '--turbo-filter=~',
    '--noopt',
    '--validate-asm',
    '--stress-validate-asm',
    '--suppress-asm-messages',
  ],
  ignition_eager=[
    '--turbo-filter=~',
    '--noopt',
    '--no-lazy',
    '--no-lazy-inner-functions',
    '--suppress-asm-messages',
  ],
  ignition_turbo=[
    '--suppress-asm-messages',
  ],
  ignition_turbo_opt=[
    '--always-opt',
    '--suppress-asm-messages',
  ],
  ignition_turbo_opt_eager=[
    '--always-opt',
    '--no-lazy',
    '--no-lazy-inner-functions',
    '--suppress-asm-messages',
  ],
  liftoff=[
    '--liftoff',
    '--suppress-asm-messages',
  ],
  slow_path=[
    '--force-slow-path',
    '--suppress-asm-messages',
  ],
  slow_path_opt=[
    '--always-opt',
    '--force-slow-path',
    '--suppress-asm-messages',
  ],
  trusted=[
    '--no-untrusted-code-mitigations',
    '--suppress-asm-messages',
  ],
  trusted_opt=[
    '--always-opt',
    '--no-untrusted-code-mitigations',
    '--suppress-asm-messages',
  ],
)

# Additional flag experiments. List of tuples like
# (<likelihood to use flags in [0,1)>, <flag>).
ADDITIONAL_FLAGS = [
  (0.1, '--stress-marking=100'),
  (0.1, '--stress-scavenge=100'),
  (0.1, '--stress-compaction-random'),
  (0.1, '--random-gc-interval=2000'),
  (0.2, '--noanalyze-environment-liveness'),
]

# Timeout in seconds for one d8 run.
TIMEOUT = 3

# Return codes.
RETURN_PASS = 0
RETURN_FAIL = 2

BASE_PATH = os.path.dirname(os.path.abspath(__file__))
PREAMBLE = [
  os.path.join(BASE_PATH, 'v8_mock.js'),
  os.path.join(BASE_PATH, 'v8_suppressions.js'),
]
ARCH_MOCKS = os.path.join(BASE_PATH, 'v8_mock_archs.js')

FLAGS = ['--abort_on_stack_or_string_length_overflow', '--expose-gc',
         '--allow-natives-syntax', '--invoke-weak-callbacks', '--omit-quit',
         '--es-staging', '--wasm-num-compilation-tasks=0']

SUPPORTED_ARCHS = ['ia32', 'x64', 'arm', 'arm64']

# Output for suppressed failure case.
FAILURE_HEADER_TEMPLATE = """#
# V8 correctness failure
# V8 correctness configs: %(configs)s
# V8 correctness sources: %(source_key)s
# V8 correctness suppression: %(suppression)s
"""

# Extended output for failure case. The 'CHECK' is for the minimizer.
FAILURE_TEMPLATE = FAILURE_HEADER_TEMPLATE + """#
# CHECK
#
# Compared %(first_config_label)s with %(second_config_label)s
#
# Flags of %(first_config_label)s:
%(first_config_flags)s
# Flags of %(second_config_label)s:
%(second_config_flags)s
#
# Difference:
%(difference)s
#
# Source file:
%(source)s
#
### Start of configuration %(first_config_label)s:
%(first_config_output)s
### End of configuration %(first_config_label)s
#
### Start of configuration %(second_config_label)s:
%(second_config_output)s
### End of configuration %(second_config_label)s
"""

FUZZ_TEST_RE = re.compile(r'.*fuzz(-\d+\.js)')
SOURCE_RE = re.compile(r'print\("v8-foozzie source: (.*)"\);')

# The number of hex digits used from the hash of the original source file path.
# Keep the number small to avoid duplicate explosion.
ORIGINAL_SOURCE_HASH_LENGTH = 3

# Placeholder string if no original source file could be determined.
ORIGINAL_SOURCE_DEFAULT = 'none'


def infer_arch(d8):
  """Infer the V8 architecture from the build configuration next to the
  executable.
  """
  with open(os.path.join(os.path.dirname(d8), 'v8_build_config.json')) as f:
    arch = json.load(f)['v8_current_cpu']
  return 'ia32' if arch == 'x86' else arch


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument(
    '--random-seed', type=int, required=True,
    help='random seed passed to both runs')
  parser.add_argument(
      '--first-config', help='first configuration', default='ignition')
  parser.add_argument(
      '--second-config', help='second configuration', default='ignition_turbo')
  parser.add_argument(
      '--first-d8', default='d8',
      help='optional path to first d8 executable, '
           'default: bundled in the same directory as this script')
  parser.add_argument(
      '--second-d8',
      help='optional path to second d8 executable, default: same as first')
  parser.add_argument('testcase', help='path to test case')
  options = parser.parse_args()

  # Ensure we have a test case.
  assert (os.path.exists(options.testcase) and
          os.path.isfile(options.testcase)), (
      'Test case %s doesn\'t exist' % options.testcase)

  # Use first d8 as default for second d8.
  options.second_d8 = options.second_d8 or options.first_d8

  # Ensure absolute paths.
  if not os.path.isabs(options.first_d8):
    options.first_d8 = os.path.join(BASE_PATH, options.first_d8)
  if not os.path.isabs(options.second_d8):
    options.second_d8 = os.path.join(BASE_PATH, options.second_d8)

  # Ensure executables exist.
  assert os.path.exists(options.first_d8)
  assert os.path.exists(options.second_d8)

  # Infer architecture from build artifacts.
  options.first_arch = infer_arch(options.first_d8)
  options.second_arch = infer_arch(options.second_d8)

  # Ensure we make a sane comparison.
  if (options.first_arch == options.second_arch and
      options.first_config == options.second_config):
    parser.error('Need either arch or config difference.')
  assert options.first_arch in SUPPORTED_ARCHS
  assert options.second_arch in SUPPORTED_ARCHS
  assert options.first_config in CONFIGS
  assert options.second_config in CONFIGS

  return options


def get_meta_data(content):
  """Extracts original-source-file paths from test case content."""
  sources = []
  for line in content.splitlines():
    match = SOURCE_RE.match(line)
    if match:
      sources.append(match.group(1))
  return {'sources': sources}


def content_bailout(content, ignore_fun):
  """Print failure state and return if ignore_fun matches content."""
  bug = (ignore_fun(content) or '').strip()
  if bug:
    print FAILURE_HEADER_TEMPLATE % dict(
        configs='', source_key='', suppression=bug)
    return True
  return False


def pass_bailout(output, step_number):
  """Print info and return if in timeout or crash pass states."""
  if output.HasTimedOut():
    # Dashed output, so that no other clusterfuzz tools can match the
    # words timeout or crash.
    print '# V8 correctness - T-I-M-E-O-U-T %d' % step_number
    return True
  if output.HasCrashed():
    print '# V8 correctness - C-R-A-S-H %d' % step_number
    return True
  return False


def fail_bailout(output, ignore_by_output_fun):
  """Print failure state and return if ignore_by_output_fun matches output."""
  bug = (ignore_by_output_fun(output.stdout) or '').strip()
  if bug:
    print FAILURE_HEADER_TEMPLATE % dict(
        configs='', source_key='', suppression=bug)
    return True
  return False


def main():
  options = parse_args()
  rng = random.Random(options.random_seed)

  # Suppressions are architecture and configuration specific.
  suppress = v8_suppressions.get_suppression(
      options.first_arch, options.first_config,
      options.second_arch, options.second_config,
  )

  # Static bailout based on test case content or metadata.
  with open(options.testcase) as f:
    content = f.read()
  if content_bailout(get_meta_data(content), suppress.ignore_by_metadata):
    return RETURN_FAIL
  if content_bailout(content, suppress.ignore_by_content):
    return RETURN_FAIL

  # Set up runtime arguments.
  common_flags = FLAGS + ['--random-seed', str(options.random_seed)]
  first_config_flags = common_flags + CONFIGS[options.first_config]
  second_config_flags = common_flags + CONFIGS[options.second_config]

  # Add additional flags to second config based on experiment percentages.
  for p, flag in ADDITIONAL_FLAGS:
    if rng.random() < p:
      second_config_flags.append(flag)

  def run_d8(d8, config_flags):
    preamble = PREAMBLE[:]
    if options.first_arch != options.second_arch:
      preamble.append(ARCH_MOCKS)
    args = [d8] + config_flags + preamble + [options.testcase]
    print " ".join(args)
    if d8.endswith('.py'):
      # Wrap with python in tests.
      args = [sys.executable] + args
    return v8_commands.Execute(
        args,
        cwd=os.path.dirname(os.path.abspath(options.testcase)),
        timeout=TIMEOUT,
    )

  first_config_output = run_d8(options.first_d8, first_config_flags)

  # Early bailout based on first run's output.
  if pass_bailout(first_config_output, 1):
    return RETURN_PASS

  second_config_output = run_d8(options.second_d8, second_config_flags)

  # Bailout based on second run's output.
  if pass_bailout(second_config_output, 2):
    return RETURN_PASS

  difference, source = suppress.diff(
      first_config_output.stdout, second_config_output.stdout)

  if source:
    source_key = hashlib.sha1(source).hexdigest()[:ORIGINAL_SOURCE_HASH_LENGTH]
  else:
    source = ORIGINAL_SOURCE_DEFAULT
    source_key = ORIGINAL_SOURCE_DEFAULT

  if difference:
    # Only bail out due to suppressed output if there was a difference. If a
    # suppression doesn't show up anymore in the statistics, we might want to
    # remove it.
    if fail_bailout(first_config_output, suppress.ignore_by_output1):
      return RETURN_FAIL
    if fail_bailout(second_config_output, suppress.ignore_by_output2):
      return RETURN_FAIL

    # The first three entries will be parsed by clusterfuzz. Format changes
    # will require changes on the clusterfuzz side.
    first_config_label = '%s,%s' % (options.first_arch, options.first_config)
    second_config_label = '%s,%s' % (options.second_arch, options.second_config)
    print (FAILURE_TEMPLATE % dict(
        configs='%s:%s' % (first_config_label, second_config_label),
        source_key=source_key,
        suppression='', # We can't tie bugs to differences.
        first_config_label=first_config_label,
        second_config_label=second_config_label,
        first_config_flags=' '.join(first_config_flags),
        second_config_flags=' '.join(second_config_flags),
        first_config_output=
            first_config_output.stdout.decode('utf-8', 'replace'),
        second_config_output=
            second_config_output.stdout.decode('utf-8', 'replace'),
        source=source,
        difference=difference.decode('utf-8', 'replace'),
    )).encode('utf-8', 'replace')
    return RETURN_FAIL

  # TODO(machenbach): Figure out if we could also return a bug in case there's
  # no difference, but one of the line suppressions has matched - and without
  # the match there would be a difference.

  print '# V8 correctness - pass'
  return RETURN_PASS


if __name__ == "__main__":
  try:
    result = main()
  except SystemExit:
    # Make sure clusterfuzz reports internal errors and wrong usage.
    # Use one label for all internal and usage errors.
    print FAILURE_HEADER_TEMPLATE % dict(
        configs='', source_key='', suppression='wrong_usage')
    result = RETURN_FAIL
  except MemoryError:
    # Running out of memory happens occasionally but is not actionable.
    print '# V8 correctness - pass'
    result = RETURN_PASS
  except Exception as e:
    print FAILURE_HEADER_TEMPLATE % dict(
        configs='', source_key='', suppression='internal_error')
    print '# Internal error: %s' % e
    traceback.print_exc(file=sys.stdout)
    result = RETURN_FAIL

  sys.exit(result)
