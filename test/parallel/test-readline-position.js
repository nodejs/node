'use strict';
require('../common');
const { PassThrough } = require('stream');
const readline = require('readline');
const assert = require('assert');

const ctrlU = { ctrl: true, name: 'u' };

{
  const input = new PassThrough();
  const rl = readline.createInterface({
    terminal: true,
    input: input,
    prompt: ''
  });

  for (const [cursor, string] of [
    [1, 'a'],
    [2, 'ab'],
    [2, '丁'],
    [0, '\u0301'],   // COMBINING ACUTE ACCENT
    [1, 'a\u0301'],  // á
    [0, '\u20DD'],   // COMBINING ENCLOSING CIRCLE
    [2, 'a\u20DDb'], // a⃝b
    [0, '\u200E']    // LEFT-TO-RIGHT MARK
  ]) {
    rl.write(string);
    assert.strictEqual(rl._getCursorPos().cols, cursor);
    rl.write(null, ctrlU);
  }
}
