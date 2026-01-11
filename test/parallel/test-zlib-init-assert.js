'use strict';

const common = require('../common');
const assert = require('assert');
const zlib = require('zlib');
const { spawn } = require('child_process');

if (process.argv[2] === 'child-deflate') {
  const trigger = new Uint32Array(1);
  const d = zlib.createDeflate();
  d._handle.init(15, 6, 8, 0, trigger, () => {}, undefined);
  d._handle.writeSync(0, Buffer.from('x'), 0, 1, Buffer.alloc(32), 0, 32);
  return;
}

if (process.argv[2] === 'child-brotli') {
  const trigger = new Uint32Array(1);
  const params = new Uint32Array(10).fill(-1);
  const b = zlib.createBrotliCompress();
  b._handle.init(params, trigger, () => {});
  b._handle.writeSync(0, Buffer.from('x'), 0, 1, Buffer.alloc(32), 0, 32);
  return;
}

// zstd
if (process.argv[2] === 'child-zstd') {
  const initParams = new Uint32Array(1);
  const writeResult = new Uint32Array(1);
  const zstd = zlib.createZstdCompress();
  zstd._handle.init(initParams, undefined, writeResult, () => {});
  zstd._handle.writeSync(0, Buffer.from('x'), 0, 1, Buffer.alloc(32), 0, 32);
  return;
}

{
  const child = spawn(process.execPath, [__filename, 'child-deflate'], {
    stdio: ['inherit', 'inherit', 'pipe'],
  });

  let stderr = '';
  child.stderr.on('data', (data) => {
    stderr += data;
  });

  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, common.isWindows ? 1 : null);
    const assertionMessage = 'Assertion failed: (array->Length()) >= (2)';
    assert.ok(
      stderr.includes(assertionMessage),
      `Expected stderr to include "${assertionMessage}", but got "${stderr}"`
    );
  }));
}

{
  const child = spawn(process.execPath, [__filename, 'child-brotli'], {
    stdio: ['inherit', 'inherit', 'pipe'],
  });

  let stderr = '';
  child.stderr.on('data', (data) => {
    stderr += data;
  });

  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, common.isWindows ? 1 : null);
    const assertionMessage = 'Assertion failed: (args[1].As<Uint32Array>()->Length()) >= (2)';
    assert.ok(
      stderr.includes(assertionMessage),
      `Expected stderr to include "${assertionMessage}", but got "${stderr}"`
    );
  }));
}

{
  const child = spawn(process.execPath, [__filename, 'child-zstd'], {
    stdio: ['inherit', 'inherit', 'pipe'],
  });

  let stderr = '';
  child.stderr.on('data', (data) => {
    stderr += data;
  });

  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, common.isWindows ? 1 : null);
    const assertionMessage = 'Assertion failed: (args[2].As<Uint32Array>()->Length()) >= (2)';
    assert.ok(
      stderr.includes(assertionMessage),
      `Expected stderr to include "${assertionMessage}", but got "${stderr}"`
    );
  }));
}
