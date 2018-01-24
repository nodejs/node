# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import optparse
import os
import sys


# Add testrunner to the path.
sys.path.insert(
  0,
  os.path.dirname(
    os.path.dirname(os.path.abspath(__file__))))


from local import utils


BASE_DIR = (
    os.path.dirname(
      os.path.dirname(
        os.path.dirname(
          os.path.abspath(__file__)))))

DEFAULT_OUT_GN = 'out.gn'

ARCH_GUESS = utils.DefaultArch()

# Map of test name synonyms to lists of test suites. Should be ordered by
# expected runtimes (suites with slow test cases first). These groups are
# invoked in separate steps on the bots.
TEST_MAP = {
  # This needs to stay in sync with test/bot_default.isolate.
  "bot_default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "webkit",
    "mkgrokdump",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/default.isolate.
  "default": [
    "debugger",
    "mjsunit",
    "cctest",
    "wasm-spec-tests",
    "inspector",
    "mkgrokdump",
    "fuzzer",
    "message",
    "preparser",
    "intl",
    "unittests",
  ],
  # This needs to stay in sync with test/optimize_for_size.isolate.
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


class ModeConfig(object):
  def __init__(self, flags, timeout_scalefactor, status_mode, execution_mode):
    self.flags = flags
    self.timeout_scalefactor = timeout_scalefactor
    self.status_mode = status_mode
    self.execution_mode = execution_mode


DEBUG_FLAGS = ["--nohard-abort", "--enable-slow-asserts", "--verify-heap"]
RELEASE_FLAGS = ["--nohard-abort"]
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


class TestRunnerError(Exception):
  pass


class BuildConfig(object):
  def __init__(self, build_config):
    # In V8 land, GN's x86 is called ia32.
    if build_config['v8_target_cpu'] == 'x86':
      self.arch = 'ia32'
    else:
      self.arch = build_config['v8_target_cpu']

    self.is_debug = build_config['is_debug']
    self.asan = build_config['is_asan']
    self.cfi_vptr = build_config['is_cfi']
    self.dcheck_always_on = build_config['dcheck_always_on']
    self.gcov_coverage = build_config['is_gcov_coverage']
    self.msan = build_config['is_msan']
    self.no_i18n = not build_config['v8_enable_i18n_support']
    self.no_snap = not build_config['v8_use_snapshot']
    self.predictable = build_config['v8_enable_verify_predictable']
    self.tsan = build_config['is_tsan']
    self.ubsan_vptr = build_config['is_ubsan_vptr']

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

    return '\n'.join(detected_options)


class BaseTestRunner(object):
  def __init__(self):
    self.outdir = None
    self.build_config = None
    self.mode_name = None
    self.mode_options = None

  def execute(self):
    try:
      parser = self._create_parser()
      options, args = self._parse_args(parser)

      self._load_build_config(options)

      try:
        self._process_default_options(options)
        self._process_options(options)
      except TestRunnerError:
        parser.print_help()
        raise

      self._setup_env()
      return self._do_execute(options, args)
    except TestRunnerError:
      return 1

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
    parser.add_option("--buildbot", help="DEPRECATED!",
                      default=False, action="store_true")
    parser.add_option("--arch",
                      help="The architecture to run tests for")
    parser.add_option("-m", "--mode",
                      help="The test mode in which to run (uppercase for ninja"
                      " and buildbot builds): %s" % MODES.keys())
    parser.add_option("--shell-dir", help="DEPRECATED! Executables from build "
                      "directory will be used")
    parser.add_option("-v", "--verbose", help="Verbose output",
                      default=False, action="store_true")

  def _add_parser_options(self, parser):
    pass

  def _parse_args(self, parser):
    options, args = parser.parse_args()

    if any(map(lambda v: v and ',' in v,
                [options.arch, options.mode])):
      print 'Multiple arch/mode are deprecated'
      raise TestRunnerError()

    return options, args

  def _load_build_config(self, options):
    for outdir in self._possible_outdirs(options):
      try:
        self.build_config = self._do_load_build_config(outdir, options.verbose)
      except TestRunnerError:
        pass

    if not self.build_config:
      print 'Failed to load build config'
      raise TestRunnerError

    print 'Build found: %s' % self.outdir
    if str(self.build_config):
      print '>>> Autodetected:'
      print self.build_config

  # Returns possible build paths in order:
  # gn
  # outdir
  # outdir/arch.mode
  # Each path is provided in two versions: <path> and <path>/mode for buildbot.
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
      yield os.path.join(BASE_DIR, outdir)

      # buildbot option
      if options.mode:
        yield os.path.join(BASE_DIR, outdir, options.mode)

  def _get_gn_outdir(self):
    gn_out_dir = os.path.join(BASE_DIR, DEFAULT_OUT_GN)
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
      except Exception:
        print("%s exists but contains invalid json. Is your build up-to-date?"
              % build_config_path)
        raise TestRunnerError()

    # In auto-detect mode the outdir is always where we found the build config.
    # This ensures that we'll also take the build products from there.
    self.outdir = os.path.dirname(build_config_path)

    return BuildConfig(build_config_json)

  def _process_default_options(self, options):
    # We don't use the mode for more path-magic.
    # Therefore transform the buildbot mode here to fix build_config value.
    if options.mode:
      options.mode = self._buildbot_to_v8_mode(options.mode)

    build_config_mode = 'debug' if self.build_config.is_debug else 'release'
    if options.mode:
      if options.mode not in MODES:
        print '%s mode is invalid' % options.mode
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

    if options.shell_dir:
      print('Warning: --shell-dir is deprecated. Searching for executables in '
            'build directory (%s) instead.' % self.outdir)

  def _buildbot_to_v8_mode(self, config):
    """Convert buildbot build configs to configs understood by the v8 runner.

    V8 configs are always lower case and without the additional _x64 suffix
    for 64 bit builds on windows with ninja.
    """
    mode = config[:-4] if config.endswith('_x64') else config
    return mode.lower()

  def _process_options(self, options):
    pass

  def _setup_env(self):
    # Use the v8 root as cwd as some test cases use "load" with relative paths.
    os.chdir(BASE_DIR)

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
          BASE_DIR,
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
        BASE_DIR,
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


  # TODO(majeski): remove options & args parameters
  def _do_execute(self, options, args):
    raise NotImplementedError()
