'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

common.skipIfInspectorDisabled();

const inputStream = new ArrayStream();
const outputStream = new ArrayStream();
repl.start({
  input: inputStream,
  output: outputStream,
  useGlobal: false,
  terminal: true,
  useColors: true
});

let output = '';
outputStream.write = (chunk) => output += chunk;

const testChars = ['\\n', '\\v', '\\r'];

for (const test of testChars) {
  inputStream.emit('data', `"${test}"()`);
  // Make sure the output is on a single line
  assert.strictEqual(output, `"${test}"()\n\x1B[90mTypeError: "\x1B[39m\x1B[9G\x1B[1A`);
  inputStream.run(['']);
  output = '';
}
