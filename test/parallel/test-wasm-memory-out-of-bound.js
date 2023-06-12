'use strict';

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const buffer = fixtures.readSync('out-of-bound.wasm');
WebAssembly.instantiate(buffer, {}).then(common.mustCall((results) => {
  assert.throws(() => {
    results.instance.exports._start();
  }, WebAssembly.RuntimeError('memory access out of bounds'));
}));
