'use strict';
var common = require('../common'),
    assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    succeeded = 0;

// This test is only relevant on Windows.
if (!common.isWindows) {
  console.log('1..0 # Skipped: Windows specific test.');
  return;
}

function test(p) {
  var result = fs.realpathSync(p);
  assert.strictEqual(result, path.resolve(p));

  fs.realpath(p, function(err, result) {
    assert.ok(!err);
    assert.strictEqual(result, path.resolve(p));
    succeeded++;
  });
}

test('//localhost/c$/windows/system32');
test('//localhost/c$/windows');
test('//localhost/c$/');
test('\\\\localhost\\c$');
test('c:\\');
test('c:');
test(process.env.windir);

process.on('exit', function() {
  assert.strictEqual(succeeded, 7);
});
