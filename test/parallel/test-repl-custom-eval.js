'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');
const { describe, it } = require('node:test');

const repl = require('repl');

describe('repl with custom eval', () => {
  it('uses the custom eval function as expected', () => {
    const output = getReplOutput('Convert this to upper case', {
      terminal: true,
      eval: (code, _ctx, _replRes, cb) => cb(null, code.toUpperCase()),
    });
    assert.match(
      output,
      /Convert this to upper case\r\n'CONVERT THIS TO UPPER CASE\\n'/
    );
  });

  it('surfaces errors as expected', () => {
    const output = getReplOutput('Convert this to upper case', {
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

  it('provides a global context to the eval callback', async () => {
    const context = await new Promise((resolve) => {
      const r = repl.start({
        useGlobal: true,
        eval: (_cmd, context) => resolve(context),
      });
      global.foo = 'global_bar';
      r.write('\n.exit\n');
    });

    assert.strictEqual(context.foo, 'global_bar');
    delete global.foo;
  });

  it('does not access the global context if `useGlobal` is false', async () => {
    const context = await new Promise((resolve) => {
      const r = repl.start({
        useGlobal: false,
        eval: (_cmd, context) => resolve(context),
      });
      global.foo = 'global_bar';
      r.write('\n.exit\n');
    });

    assert.notStrictEqual(context.foo, 'global_bar');
    delete global.foo;
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

  it("doesn't show previews by default", () => {
    const input = "'Hello custom' + ' eval World!'";
    const output = getReplOutput(input, {
      terminal: true,
      eval: (code, _ctx, _replRes, cb) => cb(null, eval(code)),
    }, false);
    assert.strictEqual(output, input);
    assert.doesNotMatch(output, /Hello custom eval World!/);
  });

  it('does show previews if `preview` is set to `true`', () => {
    const input = "'Hello custom' + ' eval World!'";
    const output = getReplOutput(input, {
      terminal: true,
      eval: (code, _ctx, _replRes, cb) => cb(null, eval(code)),
      preview: true,
    }, false);

    const escapedInput = input.replace(/\+/g, '\\+'); // TODO: migrate to `RegExp.escape` when it's available.
    assert.match(
      output,
      new RegExp(`${escapedInput}\n// 'Hello custom eval World!'`)
    );
  });
});

function getReplOutput(input, replOptions, run = true) {
  const inputStream = new ArrayStream();
  const outputStream = new ArrayStream();

  repl.start({
    input: inputStream,
    output: outputStream,
    ...replOptions,
  });

  let output = '';
  outputStream.write = (chunk) => (output += chunk);

  inputStream.emit('data', input);

  if (run) {
    inputStream.run(['']);
  }

  return output;
}
