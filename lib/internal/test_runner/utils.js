'use strict';
const { RegExp, RegExpPrototypeExec } = primordials;
const { basename } = require('path');
const { createDeferredPromise } = require('internal/util');
const {
  codes: {
    ERR_INVALID_ARG_VALUE,
    ERR_TEST_FAILURE,
  },
  kIsNodeError,
} = require('internal/errors');

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

module.exports = {
  convertStringToRegExp,
  createDeferredCallback,
  doesPathMatchFilter,
  isSupportedFileType,
  isTestFailureError,
};
