'use strict';
require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('assert');
const repl = require('repl');

// Flags: --expose-internals --experimental-repl-await

const putIn = new ArrayStream();
repl.start({
  input: putIn,
  output: putIn,
  useGlobal: false
});

let expectedIndex = -1;
const expected = ['undefined', '> ', '1', '> '];

putIn.write = function(data) {
  assert.strict(data, expected[expectedIndex += 1]);
};

putIn.run([
  'const x = await new Promise((r) => setTimeout(() => r(1), 500));\nx;',
]);
