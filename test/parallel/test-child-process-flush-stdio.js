'use strict';
const cp = require('child_process');
const common = require('../common');
const assert = require('assert');

const p = cp.spawn('echo');

p.on('close', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
}));

p.stdout.read();

setTimeout(function() {
  p.kill();
}, 100);
