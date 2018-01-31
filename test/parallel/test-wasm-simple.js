'use strict';

const common = require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');

common.crashOnUnhandledRejection();

const buffer = fixtures.readSync('test.wasm');

assert.ok(WebAssembly.validate(buffer), 'Buffer should be valid WebAssembly');

WebAssembly.instantiate(buffer, {}).then((results) => {
  assert.strictEqual(
    results.instance.exports.addTwo(10, 20),
    30,
    'Exported function should add two numbers.',
  );
});
