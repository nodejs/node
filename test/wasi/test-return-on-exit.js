'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { WASI } = require('wasi');
const wasmDir = path.join(__dirname, 'wasm');
const modulePath = path.join(wasmDir, 'exitcode.wasm');
const buffer = fs.readFileSync(modulePath);

(async () => {
  const wasi = new WASI({ version: 'preview1', returnOnExit: true });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const { instance } = await WebAssembly.instantiate(buffer, importObject);

  assert.strictEqual(wasi.start(instance), 120);
})().then(common.mustCall());

(async () => {
  // Verify that if a WASI application throws an exception, Node rethrows it
  // properly.
  const wasi = new WASI({ version: 'preview1', returnOnExit: true });
  const patchedExit = () => { throw new Error('test error'); };
  wasi.wasiImport.proc_exit = patchedExit.bind(wasi.wasiImport);
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const { instance } = await WebAssembly.instantiate(buffer, importObject);

  assert.throws(() => {
    wasi.start(instance);
  }, /^Error: test error$/);
})().then(common.mustCall());
