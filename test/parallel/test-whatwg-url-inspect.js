'use strict';

const common = require('../common');
const util = require('util');
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
const additional_tests = require(
  path.join(common.fixturesDir, 'url-tests-additional.js'));

const allTests = additional_tests.slice();
for (const test of tests) {
  if (test.failure || typeof test === 'string') continue;
  allTests.push(test);
}

for (const test of allTests) {
  const url = test.url ? new URL(test.url) : new URL(test.input, test.base);

  for (const showHidden of [true, false]) {
    const res = util.inspect(url, {
      showHidden
    });

    const lines = res.split('\n');

    const firstLine = lines[0];
    assert.strictEqual(firstLine, 'URL {');

    const lastLine = lines[lines.length - 1];
    assert.strictEqual(lastLine, '}');

    const innerLines = lines.slice(1, lines.length - 1);
    const keys = new Set();
    for (const line of innerLines) {
      const i = line.indexOf(': ');
      const k = line.slice(0, i).trim();
      const v = line.slice(i + 2);
      assert.strictEqual(keys.has(k), false, 'duplicate key found: ' + k);
      keys.add(k);

      const hidden = new Set([
        'password',
        'cannot-be-base',
        'special'
      ]);
      if (showHidden) {
        if (!hidden.has(k)) {
          assert.strictEqual(v, url[k], k);
          continue;
        }

        if (k === 'password') {
          assert.strictEqual(v, url[k], k);
        }
        if (k === 'cannot-be-base') {
          assert.ok(v.match(/^true$|^false$/), k + ' is Boolean');
        }
        if (k === 'special') {
          assert.ok(v.match(/^true$|^false$/), k + ' is Boolean');
        }
        continue;
      }

      // showHidden is false
      if (k === 'password') {
        assert.strictEqual(v, '--------', k);
        continue;
      }
      assert.strictEqual(hidden.has(k), false, 'no hidden keys: ' + k);
      assert.strictEqual(v, url[k], k);
    }
  }
}
