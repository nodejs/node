'use strict';
// emitKeypressEvents is thoroughly tested in test-readline-keys.js.
// However, that test calls it implicitly. This is just a quick sanity check
// to verify that it works when called explicitly.

require('../common');
const assert = require('assert');
const readline = require('readline');
const PassThrough = require('stream').PassThrough;
const stream = new PassThrough();
const sequence = [];
const keys = [];

readline.emitKeypressEvents(stream);

stream.on('keypress', (s, k) => {
  sequence.push(s);
  keys.push(k);
});

stream.write('foo');

process.on('exit', () => {
  assert.deepStrictEqual(sequence, ['f', 'o', 'o']);
  assert.deepStrictEqual(keys, [
    { sequence: 'f', name: 'f', ctrl: false, meta: false, shift: false },
    { sequence: 'o', name: 'o', ctrl: false, meta: false, shift: false },
    { sequence: 'o', name: 'o', ctrl: false, meta: false, shift: false }
  ]);
});
