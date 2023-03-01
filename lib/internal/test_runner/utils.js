'use strict';
const {
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ObjectGetOwnPropertyDescriptor,
  SafePromiseAllReturnArrayLike,
  RegExp,
  RegExpPrototypeExec,
  SafeMap,
} = primordials;

const { basename } = require('path');
const { createWriteStream } = require('fs');
const { pathToFileURL } = require('internal/url');
const { createDeferredPromise } = require('internal/util');
const { getOptionValue } = require('internal/options');

const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_TEST_FAILURE,
  },
  kIsNodeError,
} = require('internal/errors');
const { compose } = require('stream');

const kMultipleCallbackInvocations = 'multipleCallbackInvocations';
const kRegExpPattern = /^\/(.*)\/([a-z]*)$/;
const kSupportedFileExtensions = /\.[cm]?js$/;
const kTestFilePattern = /((^test(-.+)?)|(.+[.\-_]test))\.[cm]?js$/;

function doesPathMatchFilter(p) {
  return RegExpPrototypeExec(kTestFilePattern, basename(p)) !== null;
}

function isSupportedFileType(p) {
  return RegExpPrototypeExec(kSupportedFileExtensions, p) !== null;
}

function createDeferredCallback() {
  let calledCount = 0;
  const { promise, resolve, reject } = createDeferredPromise();
  const cb = (err) => {
    calledCount++;

    // If the callback is called a second time, let the user know, but
    // don't let them know more than once.
    if (calledCount > 1) {
      if (calledCount === 2) {
        throw new ERR_TEST_FAILURE(
          'callback invoked multiple times',
          kMultipleCallbackInvocations,
        );
      }

      return;
    }

    if (err) {
      return reject(err);
    }

    resolve();
  };

  return { promise, cb };
}

function isTestFailureError(err) {
  return err?.code === 'ERR_TEST_FAILURE' && kIsNodeError in err;
}

function convertStringToRegExp(str, name) {
  const match = RegExpPrototypeExec(kRegExpPattern, str);
  const pattern = match?.[1] ?? str;
  const flags = match?.[2] || '';

  try {
    return new RegExp(pattern, flags);
  } catch (err) {
    const msg = err?.message;

    throw new ERR_INVALID_ARG_VALUE(
      name,
      str,
      `is an invalid regular expression.${msg ? ` ${msg}` : ''}`,
    );
  }
}

const kBuiltinDestinations = new SafeMap([
  ['stdout', process.stdout],
  ['stderr', process.stderr],
]);

const kBuiltinReporters = new SafeMap([
  ['spec', 'internal/test_runner/reporter/spec'],
  ['dot', 'internal/test_runner/reporter/dot'],
  ['tap', 'internal/test_runner/reporter/tap'],
]);

const kDefaultReporter = process.stdout.isTTY ? 'spec' : 'tap';
const kDefaultDestination = 'stdout';

function tryBuiltinReporter(name) {
  const builtinPath = kBuiltinReporters.get(name);

  if (builtinPath === undefined) {
    return;
  }

  return require(builtinPath);
}

async function getReportersMap(reporters, destinations) {
  return SafePromiseAllReturnArrayLike(reporters, async (name, i) => {
    const destination = kBuiltinDestinations.get(destinations[i]) ?? createWriteStream(destinations[i]);

    // Load the test reporter passed to --test-reporter
    let reporter = tryBuiltinReporter(name);

    if (reporter === undefined) {
      let parentURL;

      try {
        parentURL = pathToFileURL(process.cwd() + '/').href;
      } catch {
        parentURL = 'file:///';
      }

      const { esmLoader } = require('internal/process/esm_loader');
      reporter = await esmLoader.import(name, parentURL, { __proto__: null });
    }

    if (reporter?.default) {
      reporter = reporter.default;
    }

    if (reporter?.prototype && ObjectGetOwnPropertyDescriptor(reporter.prototype, 'constructor')) {
      reporter = new reporter();
    }

    if (!reporter) {
      throw new ERR_INVALID_ARG_VALUE('Reporter', name, 'is not a valid reporter');
    }

    return { __proto__: null, reporter, destination };
  });
}


async function setupTestReporters(rootTest) {
  const { reporters, destinations } = parseCommandLine();
  const reportersMap = await getReportersMap(reporters, destinations);
  for (let i = 0; i < reportersMap.length; i++) {
    const { reporter, destination } = reportersMap[i];
    compose(rootTest.reporter, reporter).pipe(destination);
  }
}

let globalTestOptions;

function parseCommandLine() {
  if (globalTestOptions) {
    return globalTestOptions;
  }

  const isTestRunner = getOptionValue('--test');
  const coverage = getOptionValue('--experimental-test-coverage');
  const isChildProcess = process.env.NODE_TEST_CONTEXT === 'child';
  let destinations;
  let reporters;
  let testNamePatterns;
  let testOnlyFlag;

  if (isChildProcess) {
    reporters = [kDefaultReporter];
    destinations = [kDefaultDestination];
  } else {
    destinations = getOptionValue('--test-reporter-destination');
    reporters = getOptionValue('--test-reporter');
    if (reporters.length === 0 && destinations.length === 0) {
      ArrayPrototypePush(reporters, kDefaultReporter);
    }

    if (reporters.length === 1 && destinations.length === 0) {
      ArrayPrototypePush(destinations, kDefaultDestination);
    }

    if (destinations.length !== reporters.length) {
      throw new ERR_INVALID_ARG_VALUE(
        '--test-reporter',
        reporters,
        'must match the number of specified \'--test-reporter-destination\'',
      );
    }
  }

  if (isTestRunner) {
    testOnlyFlag = false;
    testNamePatterns = null;
  } else {
    const testNamePatternFlag = getOptionValue('--test-name-pattern');
    testOnlyFlag = getOptionValue('--test-only');
    testNamePatterns = testNamePatternFlag?.length > 0 ?
      ArrayPrototypeMap(
        testNamePatternFlag,
        (re) => convertStringToRegExp(re, '--test-name-pattern'),
      ) : null;
  }

  globalTestOptions = {
    __proto__: null,
    isTestRunner,
    coverage,
    testOnlyFlag,
    testNamePatterns,
    reporters,
    destinations,
  };

  return globalTestOptions;
}

function countCompletedTest(test, harness = test.root.harness) {
  if (test.nesting === 0) {
    harness.counters.planned++;
  }
  if (test.reportedType === 'suite') {
    harness.counters.suites++;
    return;
  }
  // Check SKIP and TODO tests first, as those should not be counted as
  // failures.
  if (test.skipped) {
    harness.counters.skipped++;
  } else if (test.isTodo) {
    harness.counters.todo++;
  } else if (test.cancelled) {
    harness.counters.cancelled++;
  } else if (!test.passed) {
    harness.counters.failed++;
  } else {
    harness.counters.passed++;
  }
  harness.counters.all++;
}

module.exports = {
  convertStringToRegExp,
  countCompletedTest,
  createDeferredCallback,
  doesPathMatchFilter,
  isSupportedFileType,
  isTestFailureError,
  parseCommandLine,
  setupTestReporters,
};
