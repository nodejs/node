'use strict';

require('../common');
const assert = require('assert');
const readline = require('readline');
const { PassThrough } = require('stream');

// Test with skipHistory: true
{
  const input = new PassThrough();
  const output = new PassThrough();
  const rl = readline.createInterface({ input, output });

  rl.question('Password? ', { skipHistory: true }, (pw) => {
    assert.strictEqual(pw, 'secret-pass');
    assert.strictEqual(rl.history.includes('secret-pass'), false, 
    'Password should not be in history');
    rl.close();
  });

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

  rl.question('Password? ', (pw) => {
    assert.strictEqual(pw, 'normal-input');
    assert.strictEqual(rl.history.includes('normal-input'), true, 
    'Input should be in history');
    rl.close();
  });

  // Simulate user typing "normal-input" + Enter
  input.write('normal-input\n');
}
