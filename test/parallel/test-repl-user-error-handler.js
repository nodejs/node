'use strict';
const common = require('../common');
const { start } = require('node:repl');
const assert = require('node:assert');
const { PassThrough } = require('node:stream');
const { once } = require('node:events');
const test = require('node:test');
const { spawn } = require('node:child_process');
const vm = require('node:vm');

common.skipIfInspectorDisabled();

function evaluate(code, context, filename, callback) {
  try {
    callback(null, vm.runInContext(code, context, { filename }));
  } catch (err) {
    callback(err);
  }
}

function* generateCases() {
  for (const async of [false, true]) {
    for (const handleErrorReturn of ['ignore', 'print', 'unhandled', 'badvalue']) {
      if (handleErrorReturn === 'badvalue') {
        // Handled through a separate test using a child process
        continue;
      }
      yield { async, handleErrorReturn };
    }
  }
}

for (const { async, handleErrorReturn } of generateCases()) {
  test(`async: ${async}, handleErrorReturn: ${handleErrorReturn}`, async () => {
    const options = {
      input: new PassThrough(),
      output: new PassThrough().setEncoding('utf8'),
      eval: evaluate,
      handleError: common.mustCall((err) => {
        queueMicrotask(() => repl.emit('handled-error', err));
        return handleErrorReturn;
      })
    };

    let uncaughtExceptionEvent;
    if (handleErrorReturn === 'unhandled' && async) {
      process.removeAllListeners('uncaughtException'); // Remove the test runner's handler
      uncaughtExceptionEvent = once(process, 'uncaughtException');
    }

    const repl = start(options);

    let outputString = '';
    options.output.on('data', (chunk) => { outputString += chunk; });

    const errorInput = async ?
      'setImmediate(() => { throw new Error("testerror") })\n' :
      'throw new Error("testerror")\n';
    const handledErrorEvent = once(repl, 'handled-error');
    options.input.write(errorInput);

    const [err] = await handledErrorEvent;
    assert.strictEqual(err.message, 'testerror');
    const exitEvent = once(repl, 'exit');
    options.input.end('42\n');
    while (!/42/.test(outputString)) {
      await once(options.output, 'data');
    }

    if (handleErrorReturn === 'print') {
      assert.match(outputString, /testerror/);
    } else {
      assert.doesNotMatch(outputString, /testerror/);
    }

    if (uncaughtExceptionEvent) {
      const [uncaughtErr] = await uncaughtExceptionEvent;
      assert.strictEqual(uncaughtErr, err);
    }
    await exitEvent;
  });
}

test('async: true, handleErrorReturn: badvalue', async () => {
  // Can't test this the same way as the other combinations
  // since this will take the process down in a way that
  // cannot be caught.
  const proc = spawn(process.execPath, ['-e', `
    require('node:repl').start({
      handleError: () => 'badvalue'
    })
  `], { encoding: 'utf8', stdio: 'pipe' });
  proc.stdin.end('throw new Error("foo");');
  let stderr = '';
  proc.stderr.setEncoding('utf8').on('data', (data) => stderr += data);
  const [exit] = await once(proc, 'close');
  assert.strictEqual(exit, 1);
  assert.match(stderr, /ERR_INVALID_STATE.+badvalue/);
});
