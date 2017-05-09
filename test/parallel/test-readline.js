'use strict';
const common = require('../common');
const { PassThrough } = require('stream');
const readline = require('readline');
const assert = require('assert');

{
  const input = new PassThrough();
  const rl = readline.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustCall((data) => {
    assert.strictEqual(data, 'abc');
  }));

  input.end('abc');
}

{
  const input = new PassThrough();
  const rl = readline.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustNotCall('must not be called before newline'));

  input.write('abc');
}

{
  const input = new PassThrough();
  const rl = readline.createInterface({
    terminal: true,
    input: input
  });

  rl.on('line', common.mustCall((data) => {
    assert.strictEqual(data, 'abc');
  }));

  input.write('abc\n');
}
