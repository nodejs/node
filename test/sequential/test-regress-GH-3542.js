// This test is only relevant on Windows.
if (process.platform !== 'win32') {
  return process.exit(0);
}

var common = require('../common'),
    assert = require('assert'),
    fs = require('fs'),
    path = require('path'),
    succeeded = 0;

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
test('//localhost/c$/')
test('\\\\localhost\\c$')
test('c:\\');
test('c:');
test(process.env.windir);

process.on('exit', function() {
  assert.strictEqual(succeeded, 7);
});