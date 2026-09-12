'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { WASI } = require('wasi');

const wasm = fs.readFileSync(
  path.join(__dirname, '..', 'fixtures', 'wasi-uint32-int32.wasm'),
);

const kWasiUint32Expectations = [
  ['test_clock_time_get', 61],
  ['test_clock_time_get_zero', 0],
  ['test_clock_time_get_one', 0],
  ['test_clock_time_get_int32_max', 61],
  ['test_clock_time_get_uint32_min', 61],
  ['test_clock_time_get_uint32_max', 61],
];

const kWasiUint32ExportNames = kWasiUint32Expectations.map(([name]) => name);

(async () => {
  const wasi = new WASI({ version: 'preview1', returnOnExit: true });
  const importObject = { wasi_snapshot_preview1: wasi.wasiImport };
  const { instance } = await WebAssembly.instantiate(wasm, importObject);

  assert.deepStrictEqual(
    Object.keys(instance.exports).sort(),
    ['_start', 'memory', ...kWasiUint32ExportNames].sort(),
  );

  wasi.start(instance);

  for (const [name, expectedErrno] of kWasiUint32Expectations) {
    assert.strictEqual(
      instance.exports[name](),
      expectedErrno,
      name,
    );
  }
})().then(common.mustCall());
