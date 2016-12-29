'use strict';
const cp = require('child_process');
const common = require('../common');
const assert = require('assert');

// Windows' `echo` command is a built-in shell command and not an external
// executable like on *nix. The V4 API does not have the "shell" option to
// spawn that we use in V4 and later so we can't use that here.
var p;
if (common.isWindows) {
  p = cp.spawn('cmd.exe', ['/c', 'echo'], {});
} else {
  p = cp.spawn('echo', [], {});
}

p.on('close', common.mustCall(function(code, signal) {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  spawnWithReadable();
}));

p.stdout.read();

function spawnWithReadable() {
  const buffer = [];
  var p;
  if (common.isWindows) {
    p = cp.spawn('cmd.exe', ['/c', 'echo', '123'], {});
  } else {
    p = cp.spawn('echo', ['123'], {});
  }
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
