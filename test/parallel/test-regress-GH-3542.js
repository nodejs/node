'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
let succeeded = 0;

// This test is only relevant on Windows.
if (!common.isWindows) {
  common.skip('Windows specific test.');
  return;
}

function test(p) {
  var result = fs.realpathSync(p);
  assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());

  fs.realpath(p, function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result.toLowerCase(), path.resolve(p).toLowerCase());
    succeeded++;
  });
}

test('//localhost/c$/Windows/System32');
test('//localhost/c$/Windows');
test('//localhost/c$/');
test('\\\\localhost\\c$\\');
test('C:\\');
test('C:');
test(process.env.windir);

process.on('exit', function() {
  assert.strictEqual(succeeded, 7);
});
