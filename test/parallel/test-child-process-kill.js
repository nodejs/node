'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var cat = spawn(common.isWindows ? 'cmd' : 'cat');

cat.stdout.on('end', common.mustCall(function() {}));
cat.stderr.on('data', common.fail);
cat.stderr.on('end', common.mustCall(function() {}));

cat.on('exit', common.mustCall(function(code, signal) {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
}));

assert.equal(cat.killed, false);
cat.kill();
assert.equal(cat.killed, true);
