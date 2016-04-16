'use strict';
const cp = require('child_process');
const common = require('../common');
const assert = require('assert');

const p = cp.spawn('echo');

p.on('close', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  spawnWithReadable();
}));

p.stdout.read();

function spawnWithReadable() {
  const buffer = [];
  const p = cp.spawn('echo', ['123']);
  p.on('close', common.mustCall(function(code, signal) {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(Buffer.concat(buffer).toString().trim(), '123');
  }));
  p.stdout.on('readable', function() {
    let buf;
    while (buf = this.read())
      buffer.push(buf);
  });
}
