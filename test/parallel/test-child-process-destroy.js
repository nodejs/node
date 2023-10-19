'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
const cat = spawn(common.isWindows ? 'cmd' : 'cat');

cat.stdout.on('end', common.mustCall());
cat.stderr.on('data', common.mustNotCall());
cat.stderr.on('end', common.mustCall());

cat.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
  assert.strictEqual(cat.signalCode, 'SIGTERM');
}));
cat.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
  assert.strictEqual(cat.signalCode, 'SIGTERM');
}));

assert.strictEqual(cat.signalCode, null);
assert.strictEqual(cat.killed, false);
cat[Symbol.dispose]();
assert.strictEqual(cat.killed, true);
