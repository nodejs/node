'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const { isLegalPort, makeSyncWrite } = require('internal/net');

for (let n = 0; n <= 0xFFFF; n++) {
  assert(isLegalPort(n));
  assert(isLegalPort(String(n)));
  assert(`0x${n.toString(16)}`);
  assert(`0o${n.toString(8)}`);
  assert(`0b${n.toString(2)}`);
}

const bad = [-1, 'a', {}, [], false, true, 0xFFFF + 1, Infinity,
             -Infinity, NaN, undefined, null, '', ' ', 1.1, '0x',
             '-0x1', '-0o1', '-0b1', '0o', '0b'];
bad.forEach((i) => assert(!isLegalPort(i)));


function writeToStdout() {
  const ctx = {
    _handle: {
      bytesWritten: 0
    }
  };
  const syncWrite = makeSyncWrite(process.stdout.fd);
  syncWrite.call(ctx, 'hello', 'utf-8', common.mustCall((ex) => {
    assert.strictEqual(undefined, ex);
  }));
  syncWrite.call(ctx, Buffer.from('world'), 'buffer', common.mustCall((ex) => {
    assert.strictEqual(undefined, ex);
  }));
  assert.strictEqual(ctx._handle.bytesWritten, 10);
}
function writeToInvalidFD() {
  const ctx = {
    _handle: {
      bytesWritten: 0
    }
  };
  const invalidFD = -1;
  const syncWrite = makeSyncWrite(invalidFD);
  syncWrite.call(ctx, Buffer.from('world'), 'buffer', common.mustCall((ex) => {
    assert.strictEqual(ex.code, 'EBADF');
  }));
  assert.strictEqual(ctx._handle.bytesWritten, 5);
}
if (!common.isWindows) {
  // This test will crash on windows, so skip it
  writeToStdout();
  writeToInvalidFD();
}
