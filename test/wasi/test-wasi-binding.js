// Flags: --experimental-wasi-unstable-preview1
'use strict';

const common = require('../common');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const buffer = fixtures.readSync(['wasi', 'simple-wasi.wasm']);
const { WASI } = require('wasi');
const wasi = new WASI({ args: [], env: process.env });
const importObject = {
  wasi_unstable: wasi.wasiImport
};

WebAssembly.instantiate(buffer, importObject)
.then(common.mustCall((results) => {
  assert(results.instance.exports._start);
  wasi.start(results.instance);
}));
