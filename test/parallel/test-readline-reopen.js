'use strict';

// Regression test for https://github.com/nodejs/node/issues/13557
// Tests that multiple subsequent readline instances can re-use an input stream.

const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');

const input = new PassThrough();
const output = new PassThrough();

const rl1 = readline.createInterface({
  input,
  output,
  terminal: true
});

rl1.on('line', common.mustCall(rl1OnLine));

// Write a line plus the first byte of a UTF-8 multibyte character to make sure
// that it doesn’t get lost when closing the readline instance.
input.write(Buffer.concat([
  Buffer.from('foo\n'),
  Buffer.from([ 0xe2 ])  // Exactly one third of a ☃ snowman.
]));

function rl1OnLine(line) {
  assert.strictEqual(line, 'foo');
  rl1.close();
  const rl2 = readline.createInterface({
    input,
    output,
    terminal: true
  });

  rl2.on('line', common.mustCall((line) => {
    assert.strictEqual(line, '☃bar');
    rl2.close();
  }));
  input.write(Buffer.from([0x98, 0x83])); // The rest of the ☃ snowman.
  input.write('bar\n');
}
