'use strict';
const {
  ArrayPrototypePush,
  ObjectCreate,
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
          kMultipleCallbackInvocations
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
      `is an invalid regular expression.${msg ? ` ${msg}` : ''}`
    );
  }
}

const kBuiltinDestinations = new SafeMap([
  ['stdout', process.stdout],
  ['stderr', process.stderr],
]);

const kBuiltinReporters = new SafeMap([
  ['spec', 'node:test/reporter/spec'],
  ['dot', 'node:test/reporter/dot'],
  ['tap', 'node:test/reporter/tap'],
]);

const kDefaultReporter = 'tap';
const kDefaultDestination = 'stdout';

async function getReportersMap(reporters, destinations) {
  return SafePromiseAllReturnArrayLike(reporters, async (name, i) => {
    const destination = kBuiltinDestinations.get(destinations[i]) ?? createWriteStream(destinations[i]);

    // Load the test reporter passed to --test-reporter
    const reporterSpecifier = kBuiltinReporters.get(name) ?? name;
    let parentURL;
    try {
      parentURL = pathToFileURL(process.cwd() + '/').href;
    } catch {
      parentURL = 'file:///';
    }
    const { esmLoader } = require('internal/process/esm_loader');
    let reporter = await esmLoader.import(reporterSpecifier, parentURL, ObjectCreate(null));

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


async function setupTestReporters(testsStream) {
  const destinations = getOptionValue('--test-reporter-destination');
  const reporters = getOptionValue('--test-reporter');

  if (reporters.length === 0 && destinations.length === 0) {
    ArrayPrototypePush(reporters, kDefaultReporter);
  }

  if (reporters.length === 1 && destinations.length === 0) {
    ArrayPrototypePush(destinations, kDefaultDestination);
  }

  if (destinations.length !== reporters.length) {
    throw new ERR_INVALID_ARG_VALUE('--test-reporter', reporters,
                                    'must match the number of specified \'--test-reporter-destination\'');
  }

  const reportersMap = await getReportersMap(reporters, destinations);
  for (let i = 0; i < reportersMap.length; i++) {
    const { reporter, destination } = reportersMap[i];
    compose(testsStream, reporter).pipe(destination);
  }
}

module.exports = {
  convertStringToRegExp,
  createDeferredCallback,
  doesPathMatchFilter,
  isSupportedFileType,
  isTestFailureError,
  setupTestReporters,
};
