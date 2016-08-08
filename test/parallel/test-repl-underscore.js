'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const stream = require('stream');

testSloppyMode();
testStrictMode();
testResetContext();
testMagicMode();

function testSloppyMode() {
  const r = initRepl(repl.REPL_MODE_SLOPPY);

  // cannot use `let` in sloppy mode
  r.write(`_;          // initial value undefined
          var x = 10;  // evaluates to undefined
          _;           // still undefined
          y = 10;      // evaluates to 10
          _;           // 10 from last eval
          _ = 20;      // explicitly set to 20
          _;           // 20 from user input
          _ = 30;      // make sure we can set it twice and no prompt
          _;           // 30 from user input
          y = 40;      // make sure eval doesn't change _
          _;           // remains 30 from user input
          `);

  assertOutput(r.output, [
    'undefined',
    'undefined',
    'undefined',
    '10',
    '10',
    'Expression assignment to _ now disabled.',
    '20',
    '20',
    '30',
    '30',
    '40',
    '30'
  ]);
}

function testStrictMode() {
  const r = initRepl(repl.REPL_MODE_STRICT);

  r.write(`_;          // initial value undefined
          var x = 10;  // evaluates to undefined
          _;           // still undefined
          let _ = 20;  // use 'let' only in strict mode - evals to undefined
          _;           // 20 from user input
          _ = 30;      // make sure we can set it twice and no prompt
          _;           // 30 from user input
          var y = 40;  // make sure eval doesn't change _
          _;           // remains 30 from user input
          function f() { let _ = 50; } // undefined
          f();         // undefined
          _;           // remains 30 from user input
          `);

  assertOutput(r.output, [
    'undefined',
    'undefined',
    'undefined',
    'undefined',
    '20',
    '30',
    '30',
    'undefined',
    '30',
    'undefined',
    'undefined',
    '30'
  ]);
}

function testMagicMode() {
  const r = initRepl(repl.REPL_MODE_MAGIC);

  r.write(`_;          // initial value undefined
          x = 10;      //
          _;           // last eval - 10
          let _ = 20;  // undefined
          _;           // 20 from user input
          _ = 30;      // make sure we can set it twice and no prompt
          _;           // 30 from user input
          var y = 40;  // make sure eval doesn't change _
          _;           // remains 30 from user input
          function f() { let _ = 50; return _; } // undefined
          f();         // 50
          _;           // remains 30 from user input
          `);

  assertOutput(r.output, [
    'undefined',
    '10',
    '10',
    'undefined',
    '20',
    '30',
    '30',
    'undefined',
    '30',
    'undefined',
    '50',
    '30'
  ]);
}

function testResetContext() {
  const r = initRepl(repl.REPL_MODE_SLOPPY);

  r.write(`_ = 10;     // explicitly set to 10
          _;           // 10 from user input
          .clear       // Clearing context...
          _;           // remains 10
          x = 20;      // but behavior reverts to last eval
          _;           // expect 20
          `);

  assertOutput(r.output, [
    'Expression assignment to _ now disabled.',
    '10',
    '10',
    'Clearing context...',
    '10',
    '20',
    '20'
  ]);
}

function initRepl(mode) {
  const inputStream = new stream.PassThrough();
  const outputStream = new stream.PassThrough();
  outputStream.accum = '';

  outputStream.on('data', (data) => {
    outputStream.accum += data;
  });

  return repl.start({
    input: inputStream,
    output: outputStream,
    useColors: false,
    terminal: false,
    prompt: '',
    replMode: mode
  });
}

function assertOutput(output, expected) {
  const lines = output.accum.trim().split('\r\n');
  assert.deepStrictEqual(lines, expected);
}
