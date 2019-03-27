# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function
from functools import reduce

from collections import OrderedDict
import json
import multiprocessing
import optparse
import os
import shlex
import sys
import traceback



# Add testrunner to the path.
sys.path.insert(
  0,
  os.path.dirname(
    os.path.dirname(os.path.abspath(__file__))))


from testrunner.local import command
from testrunner.local import testsuite
from testrunner.local import utils
from testrunner.test_config import TestConfig
from testrunner.testproc import progress
from testrunner.testproc.rerun import RerunProc
from testrunner.testproc.shard import ShardProc
from testrunner.testproc.sigproc import SignalProc
from testrunner.testproc.timeout import TimeoutProc


BASE_DIR = (
    os.path.dirname(
      os.path.dirname(
        os.path.dirname(
          os.path.abspath(__file__)))))

DEFAULT_OUT_GN = 'out.gn'

# Map of test name synonyms to lists of test suites. Should be ordered by
# expected runtimes (suites with slow test cases first). These groups are
# invoked in separate steps on the bots.
# The mapping from names used here to GN targets (which must stay in sync)
# is defined in infra/mb/gn_isolate_map.pyl.
TEST_MAP = {
  # This needs to stay in sync with group("v8_bot_default") in test/BUILD.gn.
  "bot_default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "webkit",
    "mkgrokdump",
    "wasm-js",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
    "wasm-api-tests",
  ],
  # This needs to stay in sync with group("v8_default") in test/BUILD.gn.
  "default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "mkgrokdump",
    "wasm-js",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
    "wasm-api-tests",
  ],
  # This needs to stay in sync with group("v8_d8_default") in test/BUILD.gn.
  "d8_default": [
    "debugger",
    "mjsunit",
    "webkit",
    "message",
    "preparser",
    "intl",
  ],
  # This needs to stay in sync with "v8_optimize_for_size" in test/BUILD.gn.
  "optimize_for_size": [
    "debugger",
    "mjsunit",
    "cctest",
    "inspector",
    "webkit",
    "intl",
  ],
  "unittests": [
    "unittests",
  ],
}

# Double the timeout for these:
SLOW_ARCHS = ["arm",
              "mips",
              "mipsel",
              "mips64",
              "mips64el",
              "s390",
              "s390x",
              "arm64"]


class ModeConfig(object):
  def __init__(self, flags, timeout_scalefactor, status_mode, execution_mode):
    self.flags = flags
    self.timeout_scalefactor = timeout_scalefactor
    self.status_mode = status_mode
    self.execution_mode = execution_mode


DEBUG_FLAGS = ["--nohard-abort", "--enable-slow-asserts", "--verify-heap",
               "--testing-d8-test-runner"]
RELEASE_FLAGS = ["--nohard-abort", "--testing-d8-test-runner"]
MODES = {
  "debug": ModeConfig(
    flags=DEBUG_FLAGS,
    timeout_scalefactor=4,
    status_mode="debug",
    execution_mode="debug",
  ),
  "optdebug": ModeConfig(
    flags=DEBUG_FLAGS,
    timeout_scalefactor=4,
    status_mode="debug",
    execution_mode="debug",
  ),
  "release": ModeConfig(
    flags=RELEASE_FLAGS,
    timeout_scalefactor=1,
    status_mode="release",
    execution_mode="release",
  ),
  # Normal trybot release configuration. There, dchecks are always on which
  # implies debug is set. Hence, the status file needs to assume debug-like
  # behavior/timeouts.
  "tryrelease": ModeConfig(
    flags=RELEASE_FLAGS,
    timeout_scalefactor=1,
    status_mode="debug",
    execution_mode="release",
  ),
  # This mode requires v8 to be compiled with dchecks and slow dchecks.
  "slowrelease": ModeConfig(
    flags=RELEASE_FLAGS + ["--enable-slow-asserts"],
    timeout_scalefactor=2,
    status_mode="debug",
    execution_mode="release",
  ),
}

PROGRESS_INDICATORS = {
  'verbose': progress.VerboseProgressIndicator,
  'ci': progress.CIProgressIndicator,
  'dots': progress.DotsProgressIndicator,
  'color': progress.ColorProgressIndicator,
  'mono': progress.MonochromeProgressIndicator,
}

class TestRunnerError(Exception):
  pass


class BuildConfig(object):
  def __init__(self, build_config):
    # In V8 land, GN's x86 is called ia32.
    if build_config['v8_target_cpu'] == 'x86':
      self.arch = 'ia32'
    else:
      self.arch = build_config['v8_target_cpu']

    self.asan = build_config['is_asan']
    self.cfi_vptr = build_config['is_cfi']
    self.dcheck_always_on = build_config['dcheck_always_on']
    self.gcov_coverage = build_config['is_gcov_coverage']
    self.is_android = build_config['is_android']
    self.is_clang = build_config['is_clang']
    self.is_debug = build_config['is_debug']
    self.is_full_debug = build_config['is_full_debug']
    self.msan = build_config['is_msan']
    self.no_i18n = not build_config['v8_enable_i18n_support']
    # TODO(https://crbug.com/v8/8531)
    # 'v8_use_snapshot' was removed, 'no_snap' can be removed as well.
    self.no_snap = False
    self.predictable = build_config['v8_enable_verify_predictable']
    self.tsan = build_config['is_tsan']
    # TODO(machenbach): We only have ubsan not ubsan_vptr.
    self.ubsan_vptr = build_config['is_ubsan_vptr']
    self.embedded_builtins = build_config['v8_enable_embedded_builtins']
    self.verify_csa = build_config['v8_enable_verify_csa']
    self.lite_mode = build_config['v8_enable_lite_mode']
    self.pointer_compression = build_config['v8_enable_pointer_compression']
    # Export only for MIPS target
    if self.arch in ['mips', 'mipsel', 'mips64', 'mips64el']:
      self.mips_arch_variant = build_config['mips_arch_variant']
      self.mips_use_msa = build_config['mips_use_msa']

  @property
  def use_sanitizer(self):
    return (self.asan or self.cfi_vptr or self.msan or self.tsan or
            self.ubsan_vptr)

  def __str__(self):
    detected_options = []

    if self.asan:
      detected_options.append('asan')
    if self.cfi_vptr:
      detected_options.append('cfi_vptr')
    if self.dcheck_always_on:
      detected_options.append('dcheck_always_on')
    if self.gcov_coverage:
      detected_options.append('gcov_coverage')
    if self.msan:
      detected_options.append('msan')
    if self.no_i18n:
      detected_options.append('no_i18n')
    if self.no_snap:
      detected_options.append('no_snap')
    if self.predictable:
      detected_options.append('predictable')
    if self.tsan:
      detected_options.append('tsan')
    if self.ubsan_vptr:
      detected_options.append('ubsan_vptr')
    if self.embedded_builtins:
      detected_options.append('embedded_builtins')
    if self.verify_csa:
      detected_options.append('verify_csa')
    if self.lite_mode:
      detected_options.append('lite_mode')
    if self.pointer_compression:
      detected_options.append('pointer_compression')

    return '\n'.join(detected_options)


class BaseTestRunner(object):
  def __init__(self, basedir=None):
    self.basedir = basedir or BASE_DIR
    self.outdir = None
    self.build_config = None
    self.mode_name = None
    self.mode_options = None
    self.target_os = None

  @property
  def framework_name(self):
    """String name of the base-runner subclass, used in test results."""
    raise NotImplementedError()

  def execute(self, sys_args=None):
    if sys_args is None:  # pragma: no cover
      sys_args = sys.argv[1:]
    try:
      parser = self._create_parser()
      options, args = self._parse_args(parser, sys_args)
      if options.swarming:
        # Swarming doesn't print how isolated commands are called. Lets make
        # this less cryptic by printing it ourselves.
        print(' '.join(sys.argv))

      self._load_build_config(options)
      command.setup(self.target_os, options.device)

      try:
        self._process_default_options(options)
        self._process_options(options)
      except TestRunnerError:
        parser.print_help()
        raise

      args = self._parse_test_args(args)
      tests = self._load_testsuite_generators(args, options)
      self._setup_env()
      print(">>> Running tests for %s.%s" % (self.build_config.arch,
                                            self.mode_name))
      exit_code = self._do_execute(tests, args, options)
      if exit_code == utils.EXIT_CODE_FAILURES and options.json_test_results:
        print("Force exit code 0 after failures. Json test results file "
              "generated with failure information.")
        exit_code = utils.EXIT_CODE_PASS
      return exit_code
    except TestRunnerError:
      traceback.print_exc()
      return utils.EXIT_CODE_INTERNAL_ERROR
    except KeyboardInterrupt:
      return utils.EXIT_CODE_INTERRUPTED
    except Exception:
      traceback.print_exc()
      return utils.EXIT_CODE_INTERNAL_ERROR
    finally:
      command.tear_down()

  def _create_parser(self):
    parser = optparse.OptionParser()
    parser.usage = '%prog [options] [tests]'
    parser.description = """TESTS: %s""" % (TEST_MAP["default"])
    self._add_parser_default_options(parser)
    self._add_parser_options(parser)
    return parser

  def _add_parser_default_options(self, parser):
    parser.add_option("--gn", help="Scan out.gn for the last built"
                      " configuration",
                      default=False, action="store_true")
    parser.add_option("--outdir", help="Base directory with compile output",
                      default="out")
    parser.add_option("--arch",
                      help="The architecture to run tests for")
    parser.add_option("-m", "--mode",
                      help="The test mode in which to run (uppercase for builds"
                      " in CI): %s" % MODES.keys())
    parser.add_option("--shell-dir", help="DEPRECATED! Executables from build "
                      "directory will be used")
    parser.add_option("--test-root", help="Root directory of the test suites",
                      default=os.path.join(self.basedir, 'test'))
    parser.add_option("--total-timeout-sec", default=0, type="int",
                      help="How long should fuzzer run")
    parser.add_option("--swarming", default=False, action="store_true",
                      help="Indicates running test driver on swarming.")

    parser.add_option("-j", help="The number of parallel tasks to run",
                      default=0, type=int)
    parser.add_option("-d", "--device",
                      help="The device ID to run Android tests on. If not "
                           "given it will be autodetected.")

    # Shard
    parser.add_option("--shard-count", default=1, type=int,
                      help="Split tests into this number of shards")
    parser.add_option("--shard-run", default=1, type=int,
                      help="Run this shard from the split up tests.")

    # Progress
    parser.add_option("-p", "--progress",
                      choices=PROGRESS_INDICATORS.keys(), default="mono",
                      help="The style of progress indicator (verbose, dots, "
                           "color, mono)")
    parser.add_option("--json-test-results",
                      help="Path to a file for storing json results.")
    parser.add_option("--junitout", help="File name of the JUnit output")
    parser.add_option("--junittestsuite", default="v8tests",
                      help="The testsuite name in the JUnit output file")
    parser.add_option("--exit-after-n-failures", type="int", default=100,
                      help="Exit after the first N failures instead of "
                           "running all tests. Pass 0 to disable this feature.")
    parser.add_option("--ci-test-completion",
                      help="Path to a file for logging test completion in the "
                           "context of CI progress indicator. Ignored if "
                           "progress indicator is other than 'ci'.")

    # Rerun
    parser.add_option("--rerun-failures-count", default=0, type=int,
                      help="Number of times to rerun each failing test case. "
                           "Very slow tests will be rerun only once.")
    parser.add_option("--rerun-failures-max", default=100, type=int,
                      help="Maximum number of failing test cases to rerun")

    # Test config
    parser.add_option("--command-prefix", default="",
                      help="Prepended to each shell command used to run a test")
    parser.add_option("--extra-flags", action="append", default=[],
                      help="Additional flags to pass to each test command")
    parser.add_option("--isolates", action="store_true", default=False,
                      help="Whether to test isolates")
    parser.add_option("--no-harness", "--noharness",
                      default=False, action="store_true",
                      help="Run without test harness of a given suite")
    parser.add_option("--random-seed", default=0, type=int,
                      help="Default seed for initializing random generator")
    parser.add_option("--run-skipped", help="Also run skipped tests.",
                      default=False, action="store_true")
    parser.add_option("-t", "--timeout", default=60, type=int,
                      help="Timeout for single test in seconds")
    parser.add_option("-v", "--verbose", default=False, action="store_true",
                      help="Verbose output")

    # TODO(machenbach): Temporary options for rolling out new test runner
    # features.
    parser.add_option("--mastername", default='',
                      help="Mastername property from infrastructure. Not "
                           "setting this option indicates manual usage.")
    parser.add_option("--buildername", default='',
                      help="Buildername property from infrastructure. Not "
                           "setting this option indicates manual usage.")

  def _add_parser_options(self, parser):
    pass

  def _parse_args(self, parser, sys_args):
    options, args = parser.parse_args(sys_args)

    if any(map(lambda v: v and ',' in v,
                [options.arch, options.mode])):  # pragma: no cover
      print('Multiple arch/mode are deprecated')
      raise TestRunnerError()

    return options, args

  def _load_build_config(self, options):
    for outdir in self._possible_outdirs(options):
      try:
        self.build_config = self._do_load_build_config(outdir, options.verbose)
      except TestRunnerError:
        pass

    if not self.build_config:  # pragma: no cover
      print('Failed to load build config')
      raise TestRunnerError

    print('Build found: %s' % self.outdir)
    if str(self.build_config):
      print('>>> Autodetected:')
      print(self.build_config)

    # Represents the OS where tests are run on. Same as host OS except for
    # Android, which is determined by build output.
    if self.build_config.is_android:
      self.target_os = 'android'
    else:
      self.target_os = utils.GuessOS()

  # Returns possible build paths in order:
  # gn
  # outdir
  # outdir/arch.mode
  # Each path is provided in two versions: <path> and <path>/mode for bots.
  def _possible_outdirs(self, options):
    def outdirs():
      if options.gn:
        yield self._get_gn_outdir()
        return

      yield options.outdir
      if options.arch and options.mode:
        yield os.path.join(options.outdir,
                          '%s.%s' % (options.arch, options.mode))

    for outdir in outdirs():
      yield os.path.join(self.basedir, outdir)

      # bot option
      if options.mode:
        yield os.path.join(self.basedir, outdir, options.mode)

  def _get_gn_outdir(self):
    gn_out_dir = os.path.join(self.basedir, DEFAULT_OUT_GN)
    latest_timestamp = -1
    latest_config = None
    for gn_config in os.listdir(gn_out_dir):
      gn_config_dir = os.path.join(gn_out_dir, gn_config)
      if not os.path.isdir(gn_config_dir):
        continue
      if os.path.getmtime(gn_config_dir) > latest_timestamp:
        latest_timestamp = os.path.getmtime(gn_config_dir)
        latest_config = gn_config
    if latest_config:
      print(">>> Latest GN build found: %s" % latest_config)
      return os.path.join(DEFAULT_OUT_GN, latest_config)

  def _do_load_build_config(self, outdir, verbose=False):
    build_config_path = os.path.join(outdir, "v8_build_config.json")
    if not os.path.exists(build_config_path):
      if verbose:
        print("Didn't find build config: %s" % build_config_path)
      raise TestRunnerError()

    with open(build_config_path) as f:
      try:
        build_config_json = json.load(f)
      except Exception:  # pragma: no cover
        print("%s exists but contains invalid json. Is your build up-to-date?"
              % build_config_path)
        raise TestRunnerError()

    # In auto-detect mode the outdir is always where we found the build config.
    # This ensures that we'll also take the build products from there.
    self.outdir = os.path.dirname(build_config_path)

    return BuildConfig(build_config_json)

  def _process_default_options(self, options):
    # We don't use the mode for more path-magic.
    # Therefore transform the bot mode here to fix build_config value.
    if options.mode:
      options.mode = self._bot_to_v8_mode(options.mode)

    build_config_mode = 'debug' if self.build_config.is_debug else 'release'
    if options.mode:
      if options.mode not in MODES:  # pragma: no cover
        print('%s mode is invalid' % options.mode)
        raise TestRunnerError()
      if MODES[options.mode].execution_mode != build_config_mode:
        print ('execution mode (%s) for %s is inconsistent with build config '
               '(%s)' % (
            MODES[options.mode].execution_mode,
            options.mode,
            build_config_mode))
        raise TestRunnerError()

      self.mode_name = options.mode
    else:
      self.mode_name = build_config_mode

    self.mode_options = MODES[self.mode_name]

    if options.arch and options.arch != self.build_config.arch:
      print('--arch value (%s) inconsistent with build config (%s).' % (
        options.arch, self.build_config.arch))
      raise TestRunnerError()

    if options.shell_dir:  # pragma: no cover
      print('Warning: --shell-dir is deprecated. Searching for executables in '
            'build directory (%s) instead.' % self.outdir)

    if options.j == 0:
      if self.build_config.is_android:
        # Adb isn't happy about multi-processed file pushing.
        options.j = 1
      else:
        options.j = multiprocessing.cpu_count()

    options.command_prefix = shlex.split(options.command_prefix)
    options.extra_flags = sum(map(shlex.split, options.extra_flags), [])

  def _bot_to_v8_mode(self, config):
    """Convert build configs from bots to configs understood by the v8 runner.

    V8 configs are always lower case and without the additional _x64 suffix
    for 64 bit builds on windows with ninja.
    """
    mode = config[:-4] if config.endswith('_x64') else config
    return mode.lower()

  def _process_options(self, options):
    pass

  def _setup_env(self):
    # Use the v8 root as cwd as some test cases use "load" with relative paths.
    os.chdir(self.basedir)

    # Many tests assume an English interface.
    os.environ['LANG'] = 'en_US.UTF-8'

    symbolizer_option = self._get_external_symbolizer_option()

    if self.build_config.asan:
      asan_options = [
          symbolizer_option,
          'allow_user_segv_handler=1',
          'allocator_may_return_null=1',
      ]
      if not utils.GuessOS() in ['macos', 'windows']:
        # LSAN is not available on mac and windows.
        asan_options.append('detect_leaks=1')
      else:
        asan_options.append('detect_leaks=0')
      if utils.GuessOS() == 'windows':
        # https://crbug.com/967663
        asan_options.append('detect_stack_use_after_return=0')
      os.environ['ASAN_OPTIONS'] = ":".join(asan_options)

    if self.build_config.cfi_vptr:
      os.environ['UBSAN_OPTIONS'] = ":".join([
        'print_stacktrace=1',
        'print_summary=1',
        'symbolize=1',
        symbolizer_option,
      ])

    if self.build_config.ubsan_vptr:
      os.environ['UBSAN_OPTIONS'] = ":".join([
        'print_stacktrace=1',
        symbolizer_option,
      ])

    if self.build_config.msan:
      os.environ['MSAN_OPTIONS'] = symbolizer_option

    if self.build_config.tsan:
      suppressions_file = os.path.join(
          self.basedir,
          'tools',
          'sanitizers',
          'tsan_suppressions.txt')
      os.environ['TSAN_OPTIONS'] = " ".join([
        symbolizer_option,
        'suppressions=%s' % suppressions_file,
        'exit_code=0',
        'report_thread_leaks=0',
        'history_size=7',
        'report_destroy_locked=0',
      ])

  def _get_external_symbolizer_option(self):
    external_symbolizer_path = os.path.join(
        self.basedir,
        'third_party',
        'llvm-build',
        'Release+Asserts',
        'bin',
        'llvm-symbolizer',
    )

    if utils.IsWindows():
      # Quote, because sanitizers might confuse colon as option separator.
      external_symbolizer_path = '"%s.exe"' % external_symbolizer_path

    return 'external_symbolizer_path=%s' % external_symbolizer_path

  def _parse_test_args(self, args):
    if not args:
      args = self._get_default_suite_names()

    # Expand arguments with grouped tests. The args should reflect the list
    # of suites as otherwise filters would break.
    def expand_test_group(name):
      return TEST_MAP.get(name, [name])

    return reduce(list.__add__, map(expand_test_group, args), [])

  def _args_to_suite_names(self, args, test_root):
    # Use default tests if no test configuration was provided at the cmd line.
    all_names = set(utils.GetSuitePaths(test_root))
    args_names = OrderedDict([(arg.split('/')[0], None) for arg in args]) # set
    return [name for name in args_names if name in all_names]

  def _get_default_suite_names(self):
    return []

  def _load_testsuite_generators(self, args, options):
    names = self._args_to_suite_names(args, options.test_root)
    test_config = self._create_test_config(options)
    variables = self._get_statusfile_variables(options)

    # Head generator with no elements
    test_chain = testsuite.TestGenerator(0, [], [])
    for name in names:
      if options.verbose:
        print('>>> Loading test suite: %s' % name)
      suite = testsuite.TestSuite.Load(
          os.path.join(options.test_root, name), test_config,
          self.framework_name)

      if self._is_testsuite_supported(suite, options):
        tests = suite.load_tests_from_disk(variables)
        test_chain.merge(tests)

    return test_chain

  def _is_testsuite_supported(self, suite, options):
    """A predicate that can be overridden to filter out unsupported TestSuite
    instances (see NumFuzzer for usage)."""
    return True

  def _get_statusfile_variables(self, options):
    simd_mips = (
      self.build_config.arch in ['mipsel', 'mips', 'mips64', 'mips64el'] and
      self.build_config.mips_arch_variant == "r6" and
      self.build_config.mips_use_msa)

    mips_arch_variant = (
      self.build_config.arch in ['mipsel', 'mips', 'mips64', 'mips64el'] and
      self.build_config.mips_arch_variant)

    # TODO(machenbach): In GN we can derive simulator run from
    # target_arch != v8_target_arch in the dumped build config.
    return {
      "arch": self.build_config.arch,
      "asan": self.build_config.asan,
      "byteorder": sys.byteorder,
      "dcheck_always_on": self.build_config.dcheck_always_on,
      "deopt_fuzzer": False,
      "endurance_fuzzer": False,
      "gc_fuzzer": False,
      "gc_stress": False,
      "gcov_coverage": self.build_config.gcov_coverage,
      "isolates": options.isolates,
      "is_clang": self.build_config.is_clang,
      "is_full_debug": self.build_config.is_full_debug,
      "mips_arch_variant": mips_arch_variant,
      "mode": self.mode_options.status_mode
              if not self.build_config.dcheck_always_on
              else "debug",
      "msan": self.build_config.msan,
      "no_harness": options.no_harness,
      "no_i18n": self.build_config.no_i18n,
      "no_snap": self.build_config.no_snap,
      "novfp3": False,
      "optimize_for_size": "--optimize-for-size" in options.extra_flags,
      "predictable": self.build_config.predictable,
      "simd_mips": simd_mips,
      "simulator_run": False,
      "system": self.target_os,
      "tsan": self.build_config.tsan,
      "ubsan_vptr": self.build_config.ubsan_vptr,
      "embedded_builtins": self.build_config.embedded_builtins,
      "verify_csa": self.build_config.verify_csa,
      "lite_mode": self.build_config.lite_mode,
      "pointer_compression": self.build_config.pointer_compression,
    }

  def _create_test_config(self, options):
    timeout = options.timeout * self._timeout_scalefactor(options)
    return TestConfig(
        command_prefix=options.command_prefix,
        extra_flags=options.extra_flags,
        isolates=options.isolates,
        mode_flags=self.mode_options.flags,
        no_harness=options.no_harness,
        noi18n=self.build_config.no_i18n,
        random_seed=options.random_seed,
        run_skipped=options.run_skipped,
        shell_dir=self.outdir,
        timeout=timeout,
        verbose=options.verbose,
    )

  def _timeout_scalefactor(self, options):
    """Increases timeout for slow build configurations."""
    factor = self.mode_options.timeout_scalefactor
    if self.build_config.arch in SLOW_ARCHS:
      factor *= 4
    if self.build_config.lite_mode:
      factor *= 2
    if self.build_config.predictable:
      factor *= 4
    if self.build_config.use_sanitizer:
      factor *= 1.5
    if self.build_config.is_full_debug:
      factor *= 4

    return factor

  # TODO(majeski): remove options & args parameters
  def _do_execute(self, suites, args, options):
    raise NotImplementedError()

  def _prepare_procs(self, procs):
    procs = filter(None, procs)
    for i in range(0, len(procs) - 1):
      procs[i].connect_to(procs[i + 1])
    procs[0].setup()

  def _create_shard_proc(self, options):
    myid, count = self._get_shard_info(options)
    if count == 1:
      return None
    return ShardProc(myid - 1, count)

  def _get_shard_info(self, options):
    """
    Returns pair:
      (id of the current shard [1; number of shards], number of shards)
    """
    # Read gtest shard configuration from environment (e.g. set by swarming).
    # If none is present, use values passed on the command line.
    shard_count = int(
      os.environ.get('GTEST_TOTAL_SHARDS', options.shard_count))
    shard_run = os.environ.get('GTEST_SHARD_INDEX')
    if shard_run is not None:
      # The v8 shard_run starts at 1, while GTEST_SHARD_INDEX starts at 0.
      shard_run = int(shard_run) + 1
    else:
      shard_run = options.shard_run

    if options.shard_count > 1:
      # Log if a value was passed on the cmd line and it differs from the
      # environment variables.
      if options.shard_count != shard_count:  # pragma: no cover
        print("shard_count from cmd line differs from environment variable "
              "GTEST_TOTAL_SHARDS")
      if (options.shard_run > 1 and
          options.shard_run != shard_run):  # pragma: no cover
        print("shard_run from cmd line differs from environment variable "
              "GTEST_SHARD_INDEX")

    if shard_run < 1 or shard_run > shard_count:
      # TODO(machenbach): Turn this into an assert. If that's wrong on the
      # bots, printing will be quite useless. Or refactor this code to make
      # sure we get a return code != 0 after testing if we got here.
      print("shard-run not a valid number, should be in [1:shard-count]")
      print("defaulting back to running all tests")
      return 1, 1

    return shard_run, shard_count

  def _create_progress_indicators(self, test_count, options):
    procs = [PROGRESS_INDICATORS[options.progress]()]
    if options.junitout:
      procs.append(progress.JUnitTestProgressIndicator(options.junitout,
                                                       options.junittestsuite))
    if options.json_test_results:
      procs.append(progress.JsonTestProgressIndicator(
        self.framework_name,
        options.json_test_results,
        self.build_config.arch,
        self.mode_options.execution_mode))

    for proc in procs:
      proc.configure(options)

    for proc in procs:
      try:
        proc.set_test_count(test_count)
      except AttributeError:
        pass

    return procs

  def _create_result_tracker(self, options):
    return progress.ResultsTracker(options.exit_after_n_failures)

  def _create_timeout_proc(self, options):
    if not options.total_timeout_sec:
      return None
    return TimeoutProc(options.total_timeout_sec)

  def _create_signal_proc(self):
    return SignalProc()

  def _create_rerun_proc(self, options):
    if not options.rerun_failures_count:
      return None
    return RerunProc(options.rerun_failures_count,
                     options.rerun_failures_max)
