'use strict';

// Tests below are not from WPT.

const common = require('../common');
if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
}

const URL = require('url').URL;
const assert = require('assert');
const fixtures = require('../common/fixtures');

const tests = require(
  fixtures.path('wpt', 'url', 'resources', 'urltestdata.json')
);

const originalFailures = tests.filter((test) => test.failure);

const typeFailures = [
  { input: '' },
  { input: 'test' },
  { input: undefined },
  { input: 0 },
  { input: true },
  { input: false },
  { input: null },
  { input: new Date() },
  { input: new RegExp() },
  { input: 'test', base: null },
  { input: 'http://nodejs.org', base: null },
  { input: () => {} }
];

// See https://github.com/w3c/web-platform-tests/pull/10955
// > If `failure` is true, parsing `about:blank` against `base`
// > must give failure. This tests that the logic for converting
// > base URLs into strings properly fails the whole parsing
// > algorithm if the base URL cannot be parsed.
const aboutBlankFailures = originalFailures
  .map((test) => ({
    input: 'about:blank',
    base: test.input,
    failure: true
  }));

const failureTests = originalFailures
  .concat(typeFailures)
  .concat(aboutBlankFailures);

const expectedError = common.expectsError(
  { code: 'ERR_INVALID_URL', type: TypeError }, failureTests.length);

for (const test of failureTests) {
  assert.throws(
    () => new URL(test.input, test.base),
    (error) => {
      if (!expectedError(error))
        return false;

      // The input could be processed, so we don't do strict matching here
      const match = (`${error}`).match(/Invalid URL: (.*)$/);
      if (!match) {
        return false;
      }
      return error.input === match[1];
    });
}

const additional_tests =
  require(fixtures.path('url-tests-additional.js'));

for (const test of additional_tests) {
  const url = new URL(test.url);
  if (test.href) assert.strictEqual(url.href, test.href);
  if (test.origin) assert.strictEqual(url.origin, test.origin);
  if (test.protocol) assert.strictEqual(url.protocol, test.protocol);
  if (test.username) assert.strictEqual(url.username, test.username);
  if (test.password) assert.strictEqual(url.password, test.password);
  if (test.hostname) assert.strictEqual(url.hostname, test.hostname);
  if (test.host) assert.strictEqual(url.host, test.host);
  if (test.port !== undefined) assert.strictEqual(url.port, test.port);
  if (test.pathname) assert.strictEqual(url.pathname, test.pathname);
  if (test.search) assert.strictEqual(url.search, test.search);
  if (test.hash) assert.strictEqual(url.hash, test.hash);
}
