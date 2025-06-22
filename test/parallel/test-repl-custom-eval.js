'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const { describe, it } = require('node:test');

const repl = require('repl');

// Processes some input in a REPL instance and returns a promise that
// resolves to the produced output (as a string).
function getReplRunOutput(input, replOptions) {
  return new Promise((resolve) => {
    const inputStream = new ArrayStream();
    const outputStream = new ArrayStream();

    const testingReplPrompt = '_REPL_TESTING_PROMPT_>';

    const replServer = repl.start({
      input: inputStream,
      output: outputStream,
      prompt: testingReplPrompt,
      ...replOptions,
    });

    let output = '';

    outputStream.write = (chunk) => {
      output += chunk;
      // The prompt appears after the input has been processed
      if (output.includes(testingReplPrompt)) {
        replServer.close();
        resolve(output);
      }
    };

    inputStream.emit('data', input);

    inputStream.run(['']);
  });
}

describe('repl with custom eval', { concurrency: true }, () => {
  it('uses the custom eval function as expected', async () => {
    const output = await getReplRunOutput('Convert this to upper case', {
      terminal: true,
      eval: (code, _ctx, _replRes, cb) => cb(null, code.toUpperCase()),
    });
    assert.match(
      output,
      /Convert this to upper case\r\n'CONVERT THIS TO UPPER CASE\\n'/
    );
  });

  it('surfaces errors as expected', async () => {
    const output = await getReplRunOutput('Convert this to upper case', {
      terminal: true,
      eval: (_code, _ctx, _replRes, cb) => cb(new Error('Testing Error')),
    });
    assert.match(output, /Uncaught Error: Testing Error\n/);
  });

  it('provides a repl context to the eval callback', async () => {
    const context = await new Promise((resolve) => {
      const r = repl.start({
        eval: (_cmd, context) => resolve(context),
      });
      r.context = { foo: 'bar' };
      r.write('\n.exit\n');
    });
    assert.strictEqual(context.foo, 'bar');
  });

  it('provides the global context to the eval callback', async () => {
    const context = await new Promise((resolve) => {
      const r = repl.start({
        useGlobal: true,
        eval: (_cmd, context) => resolve(context),
      });
      global.foo = 'global_foo';
      r.write('\n.exit\n');
    });

    assert.strictEqual(context.foo, 'global_foo');
    delete global.foo;
  });

  it('inherits variables from the global context but does not use it afterwords if `useGlobal` is false', async () => {
    global.bar = 'global_bar';
    const context = await new Promise((resolve) => {
      const r = repl.start({
        useGlobal: false,
        eval: (_cmd, context) => resolve(context),
      });
      global.baz = 'global_baz';
      r.write('\n.exit\n');
    });

    assert.strictEqual(context.bar, 'global_bar');
    assert.notStrictEqual(context.baz, 'global_baz');
    delete global.bar;
    delete global.baz;
  });

  /**
   * Default preprocessor transforms
   *  function f() {}  to
   *  var f = function f() {}
   * This test ensures that original input is preserved.
   * Reference: https://github.com/nodejs/node/issues/9743
   */
  it('preserves the original input', async () => {
    const cmd = await new Promise((resolve) => {
      const r = repl.start({
        eval: (cmd) => resolve(cmd),
      });
      r.write('function f() {}\n.exit\n');
    });
    assert.strictEqual(cmd, 'function f() {}\n');
  });
});
