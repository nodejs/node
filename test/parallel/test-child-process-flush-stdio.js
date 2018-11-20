'use strict';
const common = require('../common');
const cp = require('child_process');
const assert = require('assert');

// Windows' `echo` command is a built-in shell command and not an external
// executable like on *nix
const opts = { shell: common.isWindows };

const p = cp.spawn('echo', [], opts);

p.on('close', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  spawnWithReadable();
}));

p.stdout.read();

function spawnWithReadable() {
  const buffer = [];
  const p = cp.spawn('echo', ['123'], opts);
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
