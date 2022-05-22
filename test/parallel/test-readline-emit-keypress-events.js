'use strict';
// emitKeypressEvents is thoroughly tested in test-readline-keys.js.
// However, that test calls it implicitly. This is just a quick sanity check
// to verify that it works when called explicitly.

require('../common');
const assert = require('assert');
const readline = require('readline');
const PassThrough = require('stream').PassThrough;

const expectedSequence = ['f', 'o', 'o'];
const expectedKeys = [
  { sequence: 'f', name: 'f', ctrl: false, meta: false, shift: false },
  { sequence: 'o', name: 'o', ctrl: false, meta: false, shift: false },
  { sequence: 'o', name: 'o', ctrl: false, meta: false, shift: false },
];

{
  const stream = new PassThrough();
  const sequence = [];
  const keys = [];

  readline.emitKeypressEvents(stream);
  stream.on('keypress', (s, k) => {
    sequence.push(s);
    keys.push(k);
  });
  stream.write('foo');

  assert.deepStrictEqual(sequence, expectedSequence);
  assert.deepStrictEqual(keys, expectedKeys);
}

{
  const stream = new PassThrough();
  const sequence = [];
  const keys = [];

  stream.on('keypress', (s, k) => {
    sequence.push(s);
    keys.push(k);
  });
  readline.emitKeypressEvents(stream);
  stream.write('foo');

  assert.deepStrictEqual(sequence, expectedSequence);
  assert.deepStrictEqual(keys, expectedKeys);
}

{
  const stream = new PassThrough();
  const sequence = [];
  const keys = [];
  const keypressListener = (s, k) => {
    sequence.push(s);
    keys.push(k);
  };

  stream.on('keypress', keypressListener);
  readline.emitKeypressEvents(stream);
  stream.removeListener('keypress', keypressListener);
  stream.write('foo');

  assert.deepStrictEqual(sequence, []);
  assert.deepStrictEqual(keys, []);

  stream.on('keypress', keypressListener);
  stream.write('foo');

  assert.deepStrictEqual(sequence, expectedSequence);
  assert.deepStrictEqual(keys, expectedKeys);
}
