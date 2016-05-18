'use strict';

// Refs: https://github.com/nodejs/node/issues/5832

const common = require('../common');
const url = require('url');
const assert = require('assert');
const path = require('path');

const tests = require(path.join(common.fixturesDir, 'url-tests.json'));

var failed = 0;
var attempted = 0;

tests.forEach((test) => {
  attempted++;
  // Skip comments
  if (typeof test === 'string') return;
  var parsed;

  try {
    // Attempt to parse
    parsed = url.parse(url.resolve(test.base, test.input));
    if (test.failure) {
      // If the test was supposed to fail and we didn't get an
      // error, treat it as a failure.
      failed++;
    } else {
      // Test was not supposed to fail, so we're good so far. Now
      // check the results of the parse.
      var username, password;
      try {
        assert.strictEqual(test.href, parsed.href);
        assert.strictEqual(test.protocol, parsed.protocol);
        username = parsed.auth ? parsed.auth.split(':', 2)[0] : '';
        password = parsed.auth ? parsed.auth.split(':', 2)[1] : '';
        assert.strictEqual(test.username, username);
        assert.strictEqual(test.password, password);
        assert.strictEqual(test.host, parsed.host);
        assert.strictEqual(test.hostname, parsed.hostname);
        assert.strictEqual(+test.port, +parsed.port);
        assert.strictEqual(test.pathname, parsed.pathname || '/');
        assert.strictEqual(test.search, parsed.search || '');
        assert.strictEqual(test.hash, parsed.hash || '');
      } catch (err) {
        // For now, we're just interested in the number of failures.
        failed++;
      }
    }
  } catch (err) {
    // If Parse failed and it wasn't supposed to, treat it as a failure.
    if (!test.failure)
      failed++;
  }
});

assert.ok(failed === 0, `${failed} failed tests (out of ${attempted})`);
