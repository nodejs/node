'use strict';
const common = require('../common');

if (process.argv[2] === 'child') {
  (async () => {
    const fs = require('fs');
    const path = require('path');
    const { WASI } = require('wasi');
    const wasmDir = path.join(__dirname, 'wasm');
    const modulePath = path.join(wasmDir, 'main_args.wasm');
    const buffer = fs.readFileSync(modulePath);
    const wasi = new WASI();
    const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
    const { instance } = await WebAssembly.instantiate(buffer, importObject);

    // Verify that calling the _start() export instead of start() does not
    // crash. The application will fail with a non-zero exit code.
    instance.exports._start();
  })().then(common.mustCall());
} else {
  const assert = require('assert');
  const { spawnSync } = require('child_process');
  const child = spawnSync(process.execPath, [
    '--experimental-wasi-unstable-preview1',
    '--experimental-wasm-bigint',
    '--no-warnings',
    __filename,
    'child'
  ]);

  assert.strictEqual(child.stdout.toString(), '');
  assert.strictEqual(child.stderr.toString(), '');
  assert.strictEqual(child.signal, null);
  assert.notStrictEqual(child.status, 0);
}
