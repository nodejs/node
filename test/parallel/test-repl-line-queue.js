'use strict';
// Flags: --expose-internals --experimental-repl-await

require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('assert');
const repl = require('repl');

function* expectedLines(lines) {
  for (const line of lines) {
    yield line;
  }
  throw new Error('Requested more lines than expected');
}

const putIn = new ArrayStream();
repl.start({
  input: putIn,
  output: putIn,
  useGlobal: false
});

const expectedOutput = expectedLines(['undefined\n', '> ', '1\n', '> ']);
putIn.write = function(data) {
  assert.strictEqual(data, expectedOutput.next().value);
};

putIn.run([
  'const x = await new Promise((r) => setTimeout(() => r(1), 500));\nx;',
]);
