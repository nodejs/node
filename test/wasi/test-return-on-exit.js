// Flags: --experimental-wasi-unstable-preview1 --experimental-wasm-bigint
'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { WASI } = require('wasi');
const wasi = new WASI({ returnOnExit: true });
const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
const wasmDir = path.join(__dirname, 'wasm');
const modulePath = path.join(wasmDir, 'exitcode.wasm');
const buffer = fs.readFileSync(modulePath);

(async () => {
  const { instance } = await WebAssembly.instantiate(buffer, importObject);

  assert.strictEqual(wasi.start(instance), 120);
})().then(common.mustCall());
