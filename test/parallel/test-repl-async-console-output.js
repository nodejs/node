'use strict';

// Flags: --expose-internals

// Regression test for https://github.com/nodejs/node/issues/43074.

const common = require('../common');
const assert = require('assert');
const globalConsole = require('internal/console/global');
const { startNewREPLServer } = require('../common/repl');

// Ignore terminal settings so readline uses its terminal redraw path.
process.env.TERM = '';

const prompt = '> ';
const refresh = (line, cursor) =>
  `\x1b[1G\x1b[0J${prompt}${line}\x1b[${prompt.length + cursor + 1}G`;

async function test(useGlobal) {
  const originalStdout = globalConsole._stdout;
  const {
    replServer,
    output,
    waitForIdle,
  } = startNewREPLServer({
    ignoreUndefined: true,
    preview: false,
    prompt,
    terminal: true,
    useGlobal,
  });

  if (useGlobal) {
    globalConsole._stdout = output;
  }

  try {
    output.accumulator = '';
    replServer.emit('line', "console.log('sync')");
    await waitForIdle();
    assert.strictEqual(output.accumulator, `sync\n${refresh('', 0)}`);

    const logged = Promise.withResolvers();
    replServer.context.__resetAsyncConsoleOutput = () => {
      output.accumulator = '';
    };
    replServer.context.__asyncConsoleLogged = logged.resolve;

    output.accumulator = '';
    replServer.emit(
      'line',
      'void setTimeout(() => { __resetAsyncConsoleOutput(); ' +
        "console.log('async'); __asyncConsoleLogged(); }, 0)",
    );

    // These edits are buffered while the timer is being evaluated, then
    // replayed before the event loop can invoke its callback.
    replServer.write('good');
    replServer.write('', { name: 'left' });
    replServer.write('', { name: 'left' });

    await logged.promise;

    // Match REPL-managed asynchronous errors: write the output first, then
    // redraw the editable line while preserving its contents and cursor.
    assert.strictEqual(
      output.accumulator,
      `async\n${refresh('good', 2)}`,
    );
    assert.strictEqual(replServer.line, 'good');
    assert.strictEqual(replServer.cursor, 2);
  } finally {
    delete replServer.context.__resetAsyncConsoleOutput;
    delete replServer.context.__asyncConsoleLogged;
    replServer.close();
    if (useGlobal) {
      globalConsole._stdout = originalStdout;
    }
  }
}

(async () => {
  await test(false);
  await test(true);
})().then(common.mustCall());
