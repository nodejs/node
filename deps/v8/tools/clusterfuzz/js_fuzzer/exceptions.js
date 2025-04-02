// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Blacklists for fuzzer.
 */

'use strict';

const fs = require('fs');
const path = require('path');

const random = require('./random.js');

const {generatedSloppy, generatedSoftSkipped, generatedSkipped} = require(
    './generated/exceptions.js');

const SKIPPED_FILES = [
    // Disabled for unexpected test behavior, specific to d8 shell.
    'd8-os.js',
    'd8-readbuffer.js',

    // Passes JS flags.
    'd8-arguments.js',

    // Slow tests or tests that are too large to be used as input.
    /numops-fuzz-part.*.js/,
    'regexp-pcre.js',
    'unicode-test.js',
    'unicodelctest.js',
    'unicodelctest-no-optimization.js',

    // Unsupported modules.
    /^modules.*\.js/,

    // Unsupported property escapes.
    /^regexp-property-.*\.js/,

    // Bad testcases that just loads a script that always throws errors.
    'regress-444805.js',
    'regress-crbug-489597.js',
    'regress-crbug-620253.js',

    // Just recursively loads itself.
    'regress-8510.js',
];

const SKIPPED_DIRECTORIES = [
    // Slow tests or tests that are too large to be used as input.
    'embenchen',
    'poppler',
    'sqlite',

    // Causes lots of failures.
    'test262',

    // Unavailable debug.Debug.
    'v8/test/debugger',
    'v8/test/inspector',

    // Unsupported modules.
    'v8/test/js-perf-test/Modules',

    // Contains tests expected to error out on parsing.
    'v8/test/message',

    // Needs specific dependencies for load of various tests.
    'v8/test/mjsunit/tools',

    // Unsupported e4x standard.
    'mozilla/data/e4x',

    // Bails out fast without ReadableStream support.
    'spidermonkey/non262/ReadableStream',
];

// Files used with a lower probability.
const SOFT_SKIPPED_FILES = [
    // Tests with large binary content.
    /^binaryen.*\.js/,

    // Tests slow to parse.
    // CrashTests:
    /^jquery.*\.js/,
    /^js-test-pre\.js/,
    // Spidermonkey:
    'regress-308085.js',
    'regress-74474-002.js',
    'regress-74474-003.js',
    // V8:
    'object-literal.js',
    'regress-390568195.js',
];

// Used with a lower probability if paths match.
const SOFT_SKIPPED_PATHS = [
    /webgl/,
];

// Flags that lead to false positives. Furthermore choose files using these
// flags with a lower probability, as the absence of the flags typically
// renders the tests useless.
const DISALLOWED_FLAGS_WITH_DISCOURAGED_FILES = [
    // Disallowed due to noise. We explicitly add --harmony and --js-staging
    // to trials, and all of these features are staged before launch.
    /^--harmony-(?!shipping)/,
    /^--js-(?!staging|shipping)/,

    /^--icu-data-file.*/,

    // Disallowed due to false positives.
    '--correctness-fuzzer-suppressions',
    '--expose-trigger-failure',
];

// Flags that lead to false positives or that are already passed by default.
const DISALLOWED_FLAGS = [
    // Disallowed because features prefixed with "experimental" are not
    // stabilized yet and would cause too much noise when enabled.
    /^--experimental-.*/,

    // Disallowed because they are passed explicitly on the command line.
    '--allow-natives-syntax',
    '--debug-code',
    '--disable-abortjs',
    '--enable-slow-asserts',
    '--expose-gc',
    '--expose_gc',
    '--fuzzing',
    '--omit-quit',
    '--disable-in-process-stack-traces',
    '--invoke-weak-callbacks',
    '--verify-heap',

    /^--random-seed.*/,

    // Disallowed due to false positives.
    '--check-handle-count',
    '--expose-debug-as',
    '--expose-natives-as',
    '--mock-arraybuffer-allocator',
    'natives',  // Used in conjunction with --expose-natives-as.
    /^--trace-path.*/,
];

// Flags only used with 25% probability.
const LOW_PROB_FLAGS_PROB = 0.25;
const LOW_PROB_FLAGS = [
    // Flags that lead to slow test performance.
    /^--gc-interval.*/,
    /^--deopt-every-n-times.*/,
];


// Flags printing data, leading to false positives in differential fuzzing.
const DISALLOWED_DIFFERENTIAL_FUZZ_FLAGS = [
    /^--gc-interval.*/,
    /^--perf.*/,
    /^--print.*/,
    /^--stress-runs.*/,
    /^--trace.*/,
    '--expose-externalize-string',
    '--interpreted-frames-native-stack',
    '--validate-asm',
];

// Pairs of flags that shouldn't be used together.
const CONTRADICTORY_FLAGS = [
    ['--assert-types', '--stress-concurrent-inlining'],
    ['--assert-types', '--stress-concurrent-inlining-attach-code'],
    ['--jitless', '--maglev'],
    ['--jitless', '--maglev-future'],
    ['--jitless', '--stress-maglev'],
    ['--jitless', '--stress-concurrent-inlining'],
    ['--jitless', '--stress-concurrent-inlining-attach-code'],
]

const MAX_FILE_SIZE_BYTES = 128 * 1024;  // 128KB
const MEDIUM_FILE_SIZE_BYTES = 32 * 1024;  // 32KB

function _findMatch(iterable, candidate) {
  for (const entry of iterable) {
    if (typeof entry === 'string') {
      if (entry === candidate) {
        return true;
      }
    } else {
      if (entry.test(candidate)) {
        return true;
      }
    }
  }

  return false;
}

function _doesntMatch(iterable, candidate) {
  return !_findMatch(iterable, candidate);
}

// Convert Windows path separators.
function normalize(testPath) {
  return path.normalize(testPath).replace(/\\/g, '/');
}

function isTestSkippedAbs(absPath) {
  const basename = path.basename(absPath);
  if (_findMatch(SKIPPED_FILES, basename)) {
    return true;
  }

  const normalizedTestPath = normalize(absPath);
  for (const entry of SKIPPED_DIRECTORIES) {
    if (normalizedTestPath.includes(entry))  {
      return true;
    }
  }

  // Avoid OOM/hangs through huge inputs.
  const stat = fs.statSync(absPath);
  return (stat && stat.size >= MAX_FILE_SIZE_BYTES);
}

function isTestSkippedRel(relPath) {
  return generatedSkipped.has(normalize(relPath));
}

// For testing.
function getSoftSkipped() {
  return SOFT_SKIPPED_FILES;
}

// For testing.
function getSoftSkippedPaths() {
  return SOFT_SKIPPED_PATHS;
}

// For testing.
function getGeneratedSoftSkipped() {
  return generatedSoftSkipped;
}

// For testing.
function getGeneratedSloppy() {
  return generatedSloppy;
}

function isTestSoftSkippedAbs(absPath) {
  if (_findMatch(this.getSoftSkippedPaths(), absPath)) {
    return true;
  }

  const basename = path.basename(absPath);
  if (_findMatch(this.getSoftSkipped(), basename)) {
    return true;
  }

  // Graylist medium size files.
  const stat = fs.statSync(absPath);
  return (stat && stat.size >= MEDIUM_FILE_SIZE_BYTES);
}

function isTestSoftSkippedRel(relPath) {
  return this.getGeneratedSoftSkipped().has(normalize(relPath));
}

function isTestSloppyRel(relPath) {
  return this.getGeneratedSloppy().has(normalize(relPath));
}

function filterFlags(flags) {
  return flags.filter(flag => {
    return (
        _doesntMatch(DISALLOWED_FLAGS_WITH_DISCOURAGED_FILES, flag) &&
        _doesntMatch(DISALLOWED_FLAGS, flag) &&
        (_doesntMatch(LOW_PROB_FLAGS, flag) ||
         random.choose(LOW_PROB_FLAGS_PROB)));
  });
}

function hasFlagsDiscouragingFiles(flags) {
  return flags.some(flag => {
    return _findMatch(DISALLOWED_FLAGS_WITH_DISCOURAGED_FILES, flag);
  });
}

/**
 * Randomly drops flags to resolve contradicions defined by
 * `CONTRADICTORY_FLAGS`.
 */
function resolveContradictoryFlags(flags) {
  const flagSet = new Set(flags);
  for (const [flag1, flag2] of this.CONTRADICTORY_FLAGS) {
    if (flagSet.has(flag1) && flagSet.has(flag2)) {
      flagSet.delete(random.single([flag1, flag2]));
    }
  }
  return Array.from(flagSet.values());
}

function filterDifferentialFuzzFlags(flags) {
  return flags.filter(
      flag => _doesntMatch(DISALLOWED_DIFFERENTIAL_FUZZ_FLAGS, flag));
}


module.exports = {
  CONTRADICTORY_FLAGS: CONTRADICTORY_FLAGS,
  filterDifferentialFuzzFlags: filterDifferentialFuzzFlags,
  filterFlags: filterFlags,
  getGeneratedSoftSkipped: getGeneratedSoftSkipped,
  getGeneratedSloppy: getGeneratedSloppy,
  getSoftSkipped: getSoftSkipped,
  getSoftSkippedPaths: getSoftSkippedPaths,
  hasFlagsDiscouragingFiles: hasFlagsDiscouragingFiles,
  isTestSkippedAbs: isTestSkippedAbs,
  isTestSkippedRel: isTestSkippedRel,
  isTestSoftSkippedAbs: isTestSoftSkippedAbs,
  isTestSoftSkippedRel: isTestSoftSkippedRel,
  isTestSloppyRel: isTestSloppyRel,
  resolveContradictoryFlags: resolveContradictoryFlags,
}
