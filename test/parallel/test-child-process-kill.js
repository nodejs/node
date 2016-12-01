'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const cat = spawn(common.isWindows ? 'cmd' : 'cat');

cat.stdout.on('end', common.mustCall(function() {}));
cat.stderr.on('data', common.fail);
cat.stderr.on('end', common.mustCall(function() {}));

cat.on('exit', common.mustCall(function(code, signal) {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
}));

assert.strictEqual(cat.killed, false);
cat.kill();
assert.strictEqual(cat.killed, true);
