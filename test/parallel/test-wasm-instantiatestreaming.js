'use strict';

// This test verifies that instantiateStreaming can be used with node.

require('../common');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');

const promise = fs.promises.readFile(
  fixtures.path('instantiate_streaming.wasm'));

// Using an import object just to verify that this works without anything
// needed to be specifically implemented for it to work.
const importObject = {
  test: {
    add: (arg1, arg2) => arg1 + arg2
  },
}

WebAssembly.instantiateStreaming(promise, importObject).then((results) => {
  assert.strictEqual(results.instance.exports.add(2, 3), 5);
});
