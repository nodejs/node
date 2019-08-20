'use strict';

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

const buffer = fixtures.readSync('simple.wasm');

assert.ok(WebAssembly.validate(buffer), 'Buffer should be valid WebAssembly');

WebAssembly.instantiate(buffer, {}).then((results) => {
  // Exported function should add two numbers.
  assert.strictEqual(
    results.instance.exports.add(10, 20),
    30
  );
});
