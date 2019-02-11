'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const repl = require('repl');

testWithDeprecatedBreak();

function testWithDeprecatedBreak() {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();

  outputStream.write = (data) => {
    output += data.replace('\r', '');
  };

  const input = [
    '(_ => {',
    'const x = { break: "dancing" };',
    'return x;',
    '.break',
    '10'
  ];
  let output = '';

  const r = repl.start({
    prompt: '',
    input: inputStream,
    output: outputStream,
    terminal: true,
    useColors: false
  });

  const warn = 'The .break command is deprecated';
  common.expectWarning('DeprecationWarning', warn, 'REPLACEME');

  inputStream.run(input);
  outputStream.write('\r');

  const actual = output.split('\n');

  // Validate the output, which contains terminal escape codes.
  assert.strictEqual(actual.length, 7);
  assert.ok(actual[0].endsWith(input[0]));
  assert.ok(actual[1].includes('... '));
  assert.ok(actual[1].endsWith(input[1]));
  assert.ok(actual[2].endsWith(input[2]));
  assert.ok(actual[3].endsWith(input[3]));
  assert.ok(actual[4].endsWith(input[4]));
  assert.strictEqual(actual[5], '10');
  // Ignore the last line, which is nothing but escape codes.

  r.close();
}
