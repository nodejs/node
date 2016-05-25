'use strict';

const common = require('../common');
const exec = require('child_process').exec;
const assert = require('assert');

const cmd = 'echo "hello world"';

exec(cmd, { maxBuffer: 5 }, function(err, stdout, stderr) {
  assert.ok(err);
  assert.ok(/maxBuffer/.test(err.message));
});

if (!common.isWindows) {
  const unicode = '中文测试'; // length: 12
  exec('echo ' + unicode, {
    encoding: 'utf8',
    maxBuffer: 10
  }, common.mustCall(function(err, stdout, stderr) {
    assert.strictEqual(err.message, 'stdout maxBuffer exceeded');
  }));
}
