'use strict';
const common = require('../common');
// This test is only relevant on Windows.
if (!common.isWindows)
  common.skip('Windows specific test.');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

function test(p) {
  const result = fs.realpathSync(p);
  assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());

  fs.realpath(p, common.mustCall(function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());
  }));
}

test('//localhost/c$/Windows/System32');
test('//localhost/c$/Windows');
test('//localhost/c$/');
test('\\\\localhost\\c$\\');
test('C:\\');
test('C:');
test(process.env.windir);
