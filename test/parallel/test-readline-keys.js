'use strict';
require('../common');
var PassThrough = require('stream').PassThrough;
var assert = require('assert');
var inherits = require('util').inherits;
var extend = require('util')._extend;
var Interface = require('readline').Interface;


function FakeInput() {
  PassThrough.call(this);
}
inherits(FakeInput, PassThrough);


var fi = new FakeInput();
var fo = new FakeInput();
new Interface({ input: fi, output: fo, terminal: true });

var keys = [];
fi.on('keypress', function(s, k) {
  keys.push(k);
});


function addTest(sequences, expectedKeys) {
  if (!Array.isArray(sequences)) {
    sequences = [ sequences ];
  }

  if (!Array.isArray(expectedKeys)) {
    expectedKeys = [ expectedKeys ];
  }

  expectedKeys = expectedKeys.map(function(k) {
    return k ? extend({ ctrl: false, meta: false, shift: false }, k) : k;
  });

  keys = [];

  sequences.forEach(function(sequence) {
    fi.write(sequence);
  });
  assert.deepStrictEqual(keys, expectedKeys);
}

// Simulate key interval test cases
// Returns a function that takes `next` test case and returns a thunk
// that can be called to run tests in sequence
// e.g.
// addKeyIntervalTest(..)
//   (addKeyIntervalTest(..)
//      (addKeyIntervalTest(..)(noop)))()
// where noop is a terminal function(() => {}).

const addKeyIntervalTest = (sequences, expectedKeys, interval = 550,
                            assertDelay = 550) => {
  return (next) => () => {

    if (!Array.isArray(sequences)) {
      sequences = [ sequences ];
    }

    if (!Array.isArray(expectedKeys)) {
      expectedKeys = [ expectedKeys ];
    }

    expectedKeys = expectedKeys.map(function(k) {
      return k ? extend({ ctrl: false, meta: false, shift: false }, k) : k;
    });

    const keys = [];
    fi.on('keypress', (s, k) => keys.push(k));

    const emitKeys = ([head, ...tail]) => {
      if (head) {
        fi.write(head);
        setTimeout(() => emitKeys(tail), interval);
      } else {
        setTimeout(() => {
          next();
          assert.deepStrictEqual(keys, expectedKeys);
        }, assertDelay);
      }
    };
    emitKeys(sequences);
  };
};

// regular alphanumerics
addTest('io.JS', [
  { name: 'i', sequence: 'i' },
  { name: 'o', sequence: 'o' },
  { name: undefined, sequence: '.' },
  { name: 'j', sequence: 'J', shift: true },
  { name: 's', sequence: 'S', shift: true },
]);

// named characters
addTest('\n\r\t', [
  { name: 'enter', sequence: '\n' },
  { name: 'return', sequence: '\r' },
  { name: 'tab', sequence: '\t' },
]);

// space and backspace
addTest('\b\x7f\x1b\b\x1b\x7f \x1b ', [
  { name: 'backspace', sequence: '\b' },
  { name: 'backspace', sequence: '\x7f' },
  { name: 'backspace', sequence: '\x1b\b', meta: true },
  { name: 'backspace', sequence: '\x1b\x7f', meta: true },
  { name: 'space', sequence: ' ' },
  { name: 'space', sequence: '\x1b ', meta: true },
]);

// control keys
addTest('\x01\x0b\x10', [
  { name: 'a', sequence: '\x01', ctrl: true },
  { name: 'k', sequence: '\x0b', ctrl: true },
  { name: 'p', sequence: '\x10', ctrl: true },
]);

// alt keys
addTest('a\x1baA\x1bA', [
  { name: 'a', sequence: 'a' },
  { name: 'a', sequence: '\x1ba', meta: true },
  { name: 'a', sequence: 'A', shift: true },
  { name: 'a', sequence: '\x1bA', meta: true, shift: true },
]);

// xterm/gnome
addTest('\x1bOA\x1bOB', [
  { name: 'up', sequence: '\x1bOA', code: 'OA' },
  { name: 'down', sequence: '\x1bOB', code: 'OB' },
]);

// old xterm shift-arrows
addTest('\x1bO2A\x1bO2B', [
  { name: 'up', sequence: '\x1bO2A', code: 'OA', shift: true },
  { name: 'down', sequence: '\x1bO2B', code: 'OB', shift: true },
]);

// gnome terminal
addTest('\x1b[A\x1b[B\x1b[2A\x1b[2B', [
  { name: 'up', sequence: '\x1b[A', code: '[A' },
  { name: 'down', sequence: '\x1b[B', code: '[B' },
  { name: 'up', sequence: '\x1b[2A', code: '[A', shift: true },
  { name: 'down', sequence: '\x1b[2B', code: '[B', shift: true },
]);

// rxvt
addTest('\x1b[20~\x1b[2$\x1b[2^', [
  { name: 'f9', sequence: '\x1b[20~', code: '[20~' },
  { name: 'insert', sequence: '\x1b[2$', code: '[2$', shift: true },
  { name: 'insert', sequence: '\x1b[2^', code: '[2^', ctrl: true },
]);

// xterm + modifiers
addTest('\x1b[20;5~\x1b[6;5^', [
  { name: 'f9', sequence: '\x1b[20;5~', code: '[20~', ctrl: true },
  { name: 'pagedown', sequence: '\x1b[6;5^', code: '[6^', ctrl: true },
]);

addTest('\x1b[H\x1b[5H\x1b[1;5H', [
  { name: 'home', sequence: '\x1b[H', code: '[H' },
  { name: 'home', sequence: '\x1b[5H', code: '[H', ctrl: true },
  { name: 'home', sequence: '\x1b[1;5H', code: '[H', ctrl: true },
]);

// escape sequences broken into multiple data chunks
addTest('\x1b[D\x1b[C\x1b[D\x1b[C'.split(''), [
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'right', sequence: '\x1b[C', code: '[C' },
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'right', sequence: '\x1b[C', code: '[C' },
]);

// escape sequences mixed with regular ones
addTest('\x1b[DD\x1b[2DD\x1b[2^D', [
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'd', sequence: 'D', shift: true },
  { name: 'left', sequence: '\x1b[2D', code: '[D', shift: true },
  { name: 'd', sequence: 'D', shift: true },
  { name: 'insert', sequence: '\x1b[2^', code: '[2^', ctrl: true },
  { name: 'd', sequence: 'D', shift: true },
]);

// color sequences
addTest('\x1b[31ma\x1b[39ma', [
  { name: 'undefined', sequence: '\x1b[31m', code: '[31m' },
  { name: 'a', sequence: 'a' },
  { name: 'undefined', sequence: '\x1b[39m', code: '[39m' },
  { name: 'a', sequence: 'a' },
]);

// Reduce array of addKeyIntervalTest(..) right to left
// with () => {} as initial function
const runKeyIntervalTests = [
  // escape character
  addKeyIntervalTest('\x1b', [
    { name: 'escape', sequence: '\x1b', meta: true }
  ]),
  // chain of escape characters
  addKeyIntervalTest('\x1b\x1b\x1b\x1b'.split(''), [
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true }
  ])
].reverse().reduce((acc, fn) => fn(acc), () => {});

// run key interval tests one after another
runKeyIntervalTests();
