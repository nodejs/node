'use strict';

const common = require('../common');
const URL = require('url').URL;
const path = require('path');
const assert = require('assert');

if (!common.hasIntl) {
  // A handful of the tests fail when ICU is not included.
  common.skip('missing Intl');
  return;
}

// Tests below are not from WPT.
const tests = require(path.join(common.fixturesDir, 'url-tests.json'));

for (const test of tests) {
  if (typeof test === 'string')
    continue;

  if (test.failure) {
    assert.throws(() => new URL(test.input, test.base),
                  /^TypeError: Invalid URL$/);
  }
}

const additional_tests = require(
  path.join(common.fixturesDir, 'url-tests-additional.js'));

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
