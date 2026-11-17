'use strict';
// Wasm i32 arguments with the high bit set arrive in JS as negative Int32
// numbers. WASI functions must reinterpret them as uint32 pointers rather
// than rejecting them with EINVAL.
const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const { WASI } = require('wasi');

const UVWASI_EOVERFLOW = 61;

(async () => {
  const wasi = new WASI({ version: 'preview1', returnOnExit: true });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const { instance } = await WebAssembly.instantiate(
    fixtures.readSync('wasi-high-bit-ptr.wasm'), importObject);
  wasi.start(instance);

  assert.strictEqual(instance.exports.clock_time_get_low_ptr(), 0);
  assert.strictEqual(instance.exports.clock_time_get_high_ptr(),
                     UVWASI_EOVERFLOW);
  assert.strictEqual(instance.exports.args_sizes_get_high_ptr(),
                     UVWASI_EOVERFLOW);
})().then(common.mustCall());
