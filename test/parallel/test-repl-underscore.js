'use strict';

require('../common');
const assert = require('assert');
const repl = require('repl');
const { startNewREPLServer } = require('../common/repl');

const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

testSloppyMode();
testStrictMode();
testMagicMode();
testResetContext();
testResetContextGlobal();
testError();

function testSloppyMode() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_SLOPPY,
  });

  // Cannot use `let` in sloppy mode
  replServer.write(`_;          // initial value undefined
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

  assertOutput(output, [
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
    '30',
  ]);
}

function testStrictMode() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_STRICT,
  });

  replServer.write(`_;          // initial value undefined
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

  assertOutput(output, [
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
    '30',
  ]);
}

function testMagicMode() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_MAGIC,
  });

  replServer.write(`_;          // initial value undefined
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

  assertOutput(output, [
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
    '30',
  ]);
}

function testResetContext() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_MAGIC,
  });

  replServer.write(`_ = 10;     // explicitly set to 10
          _;           // 10 from user input
          .clear       // Clearing context...
          _;           // remains 10
          x = 20;      // but behavior reverts to last eval
          _;           // expect 20
          `);

  assertOutput(output, [
    'Expression assignment to _ now disabled.',
    '10',
    '10',
    'Clearing context...',
    '10',
    '20',
    '20',
  ]);
}

function testResetContextGlobal() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    useGlobal: true,
  });

  replServer.write(`_ = 10;     // explicitly set to 10
          _;           // 10 from user input
          .clear       // No output because useGlobal is true
          _;           // remains 10
          `);

  assertOutput(output, [
    'Expression assignment to _ now disabled.',
    '10',
    '10',
    '10',
  ]);

  // Delete globals leaked by REPL when `useGlobal` is `true`
  delete globalThis.module;
  delete globalThis.require;
}

function testError() {
  const { replServer, output } = startNewREPLServer({
    prompt: testingReplPrompt,
    replMode: repl.REPL_MODE_STRICT,
    preview: false,
  }, {
    disableDomainErrorAssert: true
  });

  replServer.write(`_error;                                // initial value undefined
           throw new Error('foo');                // throws error
           _error;                                // shows error
           fs.readdirSync('/nonexistent?');       // throws error, sync
           _error.code;                           // shows error code
           _error.syscall;                        // shows error syscall
           setImmediate(() => { throw new Error('baz'); }); undefined;
                                                  // throws error, async
           `);

  setImmediate(() => {
    const lines = output.accumulator.trim().split('\n').filter(
      (line) => !line.includes(testingReplPrompt) || line.includes('Uncaught Error')
    );
    const expectedLines = [
      'undefined',

      // The error, both from the original throw and the `_error` echo.
      'Uncaught Error: foo',
      '[Error: foo]',

      // The sync error, with individual property echoes
      /^Uncaught Error: ENOENT: no such file or directory, scandir '.*nonexistent\?'/,
      /Object\.readdirSync/,
      /^ {2}errno: -(2|4058),$/,
      "  code: 'ENOENT',",
      "  syscall: 'scandir',",
      /^ {2}path: '*'/,
      '}',
      "'ENOENT'",
      "'scandir'",

      // Dummy 'undefined' from the explicit silencer + one from the comment
      'undefined',
      'undefined',

      // The message from the original throw
      /Uncaught Error: baz/,
    ];
    for (const line of lines) {
      const expected = expectedLines.shift();
      if (typeof expected === 'string')
        assert.strictEqual(line, expected);
      else
        assert.match(line, expected);
    }
    assert.strictEqual(expectedLines.length, 0);

    // Reset output, check that '_error' is the asynchronously caught error.
    output.accumulator = '';
    replServer.write(`_error.message                 // show the message
             _error = 0;                    // disable auto-assignment
             throw new Error('quux');       // new error
             _error;                        // should not see the new error
             `);

    assertOutput(output, [
      "'baz'",
      'Expression assignment to _error now disabled.',
      '0',
      'Uncaught Error: quux',
      '0',
    ]);
  });
}

function assertOutput(output, expected) {
  const lines = output.accumulator.trim().split('\n').filter((line) => !line.includes(testingReplPrompt));
  assert.deepStrictEqual(lines, expected);
}
