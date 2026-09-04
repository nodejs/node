'use strict';

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { Worker } = require('node:worker_threads');

const styled = '\u001b[31mhello\u001b[39m';
const plain = 'hello';

const workerCode = `
  const { parentPort } = require('node:worker_threads');
  const { styleText } = require('node:util');

  parentPort.postMessage(styleText('red', 'hello'));
`;

async function runWorker(options) {
  const worker = new Worker(workerCode, options);
  const exit = once(worker, 'exit');
  const [actual] = await once(worker, 'message');

  assert.deepStrictEqual(await exit, [0]);
  return actual;
}

// Make the parent destination deterministically color-capable. Worker color
// behavior should not depend on whether the test runner itself owns a TTY.
Object.defineProperty(process.stdout, 'isTTY', {
  configurable: true,
  value: true,
});
Object.defineProperty(process.stdout, 'getColorDepth', {
  configurable: true,
  value: () => 8,
});

const env = { ...process.env, TERM: 'xterm-256color' };
delete env.FORCE_COLOR;
delete env.NO_COLOR;
delete env.NODE_DISABLE_COLORS;

common.mustCall(async () => {
  const [pipedOutput, capturedOutput] = await Promise.all([
    // By default, worker output is piped to the parent's color-capable stdout.
    runWorker({ eval: true, env }),

    // With stdout: true, the output is captured and its eventual destination is
    // unknown, so styleText() should not add ANSI sequences automatically.
    runWorker({ eval: true, env, stdout: true }),
  ]);

  assert.strictEqual(capturedOutput, plain);
  assert.strictEqual(pipedOutput, styled);
})();
