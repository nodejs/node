'use strict';

const common = require('../common');
const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');

// Test with skipHistory: true
{
  const input = new PassThrough();
  const output = new PassThrough();
  const rl = readline.createInterface({ input, output });

  rl.question('Password? ', { skipHistory: true }, common.mustCall((pw) => {
    assert.strictEqual(pw, 'secret-pass');
    assert.ok(!rl.history.includes('secret-pass'), 'Password should not be in history');
    rl.close();
  }));

  // Simulate user typing "secret-pass" + Enter
  input.write('secret-pass\n');
}

// Test with skipHistory: false (default behavior)
{
  const input = new PassThrough();
  input.isTTY = true;  // simulate TTY
  const output = new PassThrough();
  output.isTTY = true; // simulate TTY
  const rl = readline.createInterface({ input, output });

  rl.question('Password? ', common.mustCall((pw) => {
    assert.strictEqual(pw, 'normal-input');
    assert.partialDeepStrictEqual(rl.history, ['normal-input']);
    rl.close();
  }));

  // Simulate user typing "normal-input" + Enter
  input.write('normal-input\n');
}
