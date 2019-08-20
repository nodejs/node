'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');
const inputStream = new ArrayStream();
const outputStream = new ArrayStream();
const input = ['var foo = {', '};', 'foo;'];
let output = '';

outputStream.write = (data) => { output += data.replace('\r', ''); };

const r = repl.start({
  prompt: '',
  input: inputStream,
  output: outputStream,
  terminal: true,
  useColors: false
});

r.on('exit', common.mustCall(() => {
  const actual = output.split('\n');

  // Validate the output, which contains terminal escape codes.
  assert.strictEqual(actual.length, 6);
  assert.ok(actual[0].endsWith(input[0]));
  assert.ok(actual[1].includes('... '));
  assert.ok(actual[1].endsWith(input[1]));
  assert.strictEqual(actual[2], 'undefined');
  assert.ok(actual[3].endsWith(input[2]));
  assert.strictEqual(actual[4], '{}');
  // Ignore the last line, which is nothing but escape codes.
}));

inputStream.run(input);
r.close();
