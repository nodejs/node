// Flags: --expose-internals
'use strict';
require('../common');
const { internalBinding } = require('internal/test/binding');
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

  const tests = [
    [1, 'a'],
    [2, 'ab'],
    [2, '丁']
  ];

  // The non-ICU JS implementation of character width calculation is only aware
  // of the wide/narrow distinction. Only test these more advanced cases when
  // ICU is available.
  if (internalBinding('config').hasIntl) {
    tests.push(
      [0, '\u0301'],   // COMBINING ACUTE ACCENT
      [1, 'a\u0301'],  // á
      [0, '\u20DD'],   // COMBINING ENCLOSING CIRCLE
      [2, 'a\u20DDb'], // a⃝b
      [0, '\u200E']    // LEFT-TO-RIGHT MARK
    );
  }

  for (const [cursor, string] of tests) {
    rl.write(string);
    assert.strictEqual(rl.getCursorPos().cols, cursor);
    rl.write(null, ctrlU);
  }
}
