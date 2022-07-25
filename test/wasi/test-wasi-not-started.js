'use strict';
require('../common');

if (process.argv[2] === 'wasi-child') {
  const assert = require('assert');
  const fs = require('fs');
  const path = require('path');

  const { WASI } = require('wasi');
  const wasi = new WASI({
    args: ['foo', '-bar', '--baz=value']
  });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };

  const modulePath = path.join(__dirname, 'wasm', 'main_args.wasm');
  const buffer = fs.readFileSync(modulePath);

  assert.rejects(async () => {
    const { instance } = await WebAssembly.instantiate(buffer, importObject);
    instance.exports._start();
  }, {
    name: 'Error',
    code: 'ERR_WASI_NOT_STARTED',
    message: 'wasi.start() has not been called'
  });
} else {
  const assert = require('assert');
  const cp = require('child_process');

  const child = cp.spawnSync(process.execPath, [
    '--experimental-wasi-unstable-preview1',
    __filename,
    'wasi-child',
  ], {
    env: { ...process.env, NODE_DEBUG_NATIVE: 'wasi' }
  });
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.status, 0);
}
