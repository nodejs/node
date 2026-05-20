'use strict';
const common = require('../common');
const { start } = require('node:repl');
const assert = require('node:assert');
const { PassThrough } = require('node:stream');
const { once } = require('node:events');
const test = require('node:test');
const { spawn } = require('node:child_process');

function* generateCases() {
  for (const async of [false, true]) {
    for (const handleErrorReturn of ['ignore', 'print', 'unhandled', 'badvalue']) {
      if (handleErrorReturn === 'badvalue' && async) {
        // Handled through a separate test using a child process
        continue;
      }
      yield { async, handleErrorReturn };
    }
  }
}

for (const { async, handleErrorReturn } of generateCases()) {
  test(`async: ${async}, handleErrorReturn: ${handleErrorReturn}`, async () => {
    let err;
    const options = {
      input: new PassThrough(),
      output: new PassThrough().setEncoding('utf8'),
      handleError: common.mustCall((e) => {
        err = e;
        queueMicrotask(() => repl.emit('handled-error'));
        return handleErrorReturn;
      })
    };

    let uncaughtExceptionEvent;
    if (handleErrorReturn === 'unhandled' && async) {
      process.removeAllListeners('uncaughtException'); // Remove the test runner's handler
      uncaughtExceptionEvent = once(process, 'uncaughtException');
    }

    const repl = start(options);
    const inputString = async ?
      'setImmediate(() => { throw new Error("testerror") })\n42\n' :
      'throw new Error("testerror")\n42\n';
    if (handleErrorReturn === 'badvalue') {
      assert.throws(() => options.input.end(inputString), /ERR_INVALID_STATE/);
      return;
    }
    options.input.end(inputString);

    await once(repl, 'handled-error');
    assert.strictEqual(err.message, 'testerror');
    const outputString = options.output.read();
    assert.match(outputString, /42/);

    if (handleErrorReturn === 'print') {
      assert.match(outputString, /testerror/);
    } else {
      assert.doesNotMatch(outputString, /testerror/);
    }

    if (uncaughtExceptionEvent) {
      const [uncaughtErr] = await uncaughtExceptionEvent;
      assert.strictEqual(uncaughtErr, err);
    }
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
