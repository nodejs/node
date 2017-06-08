'use strict';
const common = require('../common');
const cp = require('child_process');
const assert = require('assert');
const stream = require('stream');

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
    spawnWithPipe();
  }));
  p.stdout.on('readable', function() {
    setImmediate(() => {
      let buf;
      while (buf = this.read())
        buffer.push(buf);
    });
  });
}

function spawnWithPipe() {
  const buffer = [];
  const through = new stream.PassThrough();
  const p = cp.spawn('seq', [ '36000' ]);

  const ret = [];
  for (let i = 1; i <= 36000; i++) {
    ret.push(i);
  }

  p.on('close', common.mustCall(function(code, signal) {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(Buffer.concat(buffer).toString().trim(), ret.join('\n'));
  }));

  p.on('exit', common.mustCall(function() {
    setImmediate(function() {
      through.on('readable', function() {
        let buf;
        while (buf = this.read())
          buffer.push(buf);
      });
    });
  }));

  p.stdout.pipe(through);
}
