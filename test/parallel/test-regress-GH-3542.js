'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

// This test is only relevant on Windows.
if (!common.isWindows) {
  common.skip('Windows specific test.');
  return;
}

function test(p) {
  var result = fs.realpathSync(p);
  assert.strictEqual(result, path.resolve(p));

  fs.realpath(p, common.mustCall(function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result, path.resolve(p));
  }));
}

test('//localhost/c$/windows/system32');
test('//localhost/c$/windows');
test('//localhost/c$/');
test('\\\\localhost\\c$');
test('c:\\');
test('c:');
test(process.env.windir);
