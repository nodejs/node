'use strict';

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const buffer = fixtures.readSync('out-of-bound.wasm');
WebAssembly.instantiate(buffer, {}).then((results) => {
  assert.throws(() => {
    results.instance.exports._start();
  }, WebAssembly.RuntimeError('memory access out of bounds'));
});
