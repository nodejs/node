import '../common/index.mjs';
import assert from 'node:assert';
import repl from 'node:repl';
import { startNewREPLServer } from '../common/repl.js';

const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

// Feed a block of source into the REPL one line at a time, awaiting each line
// so the asynchronous evaluator settles before the next line is written.
// Blank/whitespace-only and comment-only lines produce no evaluation output
// and are skipped (the previous synchronous REPL coalesced the whole block in
// a single write, so a trailing comment line evaluated to `undefined`; feeding
// line by line we simply omit it to keep output deterministic).
async function feed(run, block) {
  for (const line of block.split('\n')) {
    const trimmed = line.trim();
    if (trimmed === '' || trimmed.startsWith('//')) continue;
    await run(`${line}\n`);
  }
}

await testSloppyMode();
await testStrictMode();
await testResetContext();
await testResetContextGlobal();
await testError();

async function testSloppyMode() {
  const { replServer, output, run } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_SLOPPY,
  });

  // Cannot use `let` in sloppy mode
  await feed(run, `_;          // initial value undefined
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

  replServer.close();
}

async function testStrictMode() {
  const { replServer, output, run } = startNewREPLServer({
    prompt: testingReplPrompt,
    mode: repl.REPL_MODE_STRICT,
  });

  await feed(run, `_;          // initial value undefined
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

  replServer.close();
}

async function testResetContext() {
  const { replServer, output, run } = startNewREPLServer({
    prompt: testingReplPrompt,
  });

  await feed(run, `_ = 10;     // explicitly set to 10
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

  replServer.close();
}

async function testResetContextGlobal() {
  const { replServer, output, run } = startNewREPLServer({
    prompt: testingReplPrompt,
    useGlobal: true,
  });

  await feed(run, `_ = 10;     // explicitly set to 10
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

  replServer.close();

  // Delete globals leaked by REPL when `useGlobal` is `true`
  delete globalThis.module;
  delete globalThis.require;
}

async function testError() {
  const { replServer, output, run } = startNewREPLServer({
    prompt: testingReplPrompt,
    replMode: repl.REPL_MODE_STRICT,
    preview: false,
  });

  await feed(run, `_error;                                // initial value undefined
           throw new Error('foo');                // throws error
           _error;                                // shows error
           fs.readdirSync('/nonexistent?');       // throws error, sync
           _error.code;                           // shows error code
           _error.syscall;                        // shows error syscall
           setImmediate(() => { throw new Error('baz'); }); undefined;
                                                  // throws error, async
           `);

  const lines = output.accumulator.trim().split('\n').filter(
    (line) => !line.includes(testingReplPrompt) || line.includes('Uncaught Error')
  );
  const expectedLines = [
    'undefined',

    // The error, both from the original throw and the `_error` echo. The
    // inspector-based evaluator returns the real Error object, so both the
    // thrown report and the echo now include a stack trace rather than the
    // compact `[Error: foo]` form produced by the previous vm-based REPL.
    'Uncaught Error: foo',
    /^ +at REPL\d+:/,
    'Error: foo',
    /^ +at REPL\d+:/,

    // The sync error, with individual property echoes. The thrown report now
    // includes stack frames before the property listing.
    /^Uncaught Error: ENOENT: no such file or directory, scandir '.*nonexistent\?'/,
    /Object\.readdirSync/,
    /^ +at REPL\d+:.*\{$/,
    /^ {2}errno: -(2|4058),$/,
    "  code: 'ENOENT',",
    "  syscall: 'scandir',",
    /^ {2}path: '*'/,
    '}',
    "'ENOENT'",
    "'scandir'",

    // 'undefined' from the explicit silencer on the setImmediate line.
    'undefined',

    // The message from the asynchronously thrown error.
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
  // Because input is now fed one line at a time and the accumulator is reset
  // mid-stream, the first line's echo is no longer preceded by a prompt and so
  // survives the prompt filter; ignore that leading echo. The thrown error also
  // now reports a stack frame.
  output.accumulator = '';
  await feed(run, `_error.message                 // show the message
             _error = 0;                    // disable auto-assignment
             throw new Error('quux');       // new error
             _error;                        // should not see the new error
             `);

  const errorLines = output.accumulator.trim().split('\n')
    .filter((line) => !line.includes(testingReplPrompt) &&
                      !line.includes('_error.message') &&
                      !/^ +at REPL\d+:/.test(line));
  assert.deepStrictEqual(errorLines, [
    "'baz'",
    'Expression assignment to _error now disabled.',
    '0',
    'Uncaught Error: quux',
    '0',
  ]);

  replServer.close();
}

function assertOutput(output, expected) {
  const lines = output.accumulator.trim().split('\n').filter((line) => !line.includes(testingReplPrompt));
  assert.deepStrictEqual(lines, expected);
}
