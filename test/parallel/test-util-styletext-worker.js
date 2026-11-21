'use strict';

const common = require('../common');
const assert = require('node:assert');
const { once } = require('node:events');
const { Worker } = require('node:worker_threads');

const styled = '\u001b[31mhello\u001b[39m';
const plain = 'hello';

const workerCode = `
  const { parentPort, workerData } = require('node:worker_threads');
  const { styleText } = require('node:util');

  const stream = process[workerData.stream];
  parentPort.postMessage({
    hasColorDepth: typeof stream.getColorDepth === 'function',
    hasColors: typeof stream.hasColors === 'function',
    isTTY: stream.isTTY,
    result: styleText('red', 'hello', { stream }),
  });
`;

const env = { ...process.env, TERM: 'xterm-256color' };
delete env.FORCE_COLOR;
delete env.NO_COLOR;
delete env.NODE_DISABLE_COLORS;

async function runWorker(stream, options = {}) {
  const worker = new Worker(workerCode, {
    env,
    eval: true,
    workerData: { stream },
    ...options,
  });
  const exit = once(worker, 'exit');
  const [actual] = await once(worker, 'message');

  assert.deepStrictEqual(await exit, [0]);
  return actual;
}

function setIsTTY(stream, value) {
  Object.defineProperty(stream, 'isTTY', {
    configurable: true,
    value,
  });
}

function restoreIsTTY(stream, descriptor) {
  if (descriptor === undefined) {
    delete stream.isTTY;
  } else {
    Object.defineProperty(stream, 'isTTY', descriptor);
  }
}

const stdoutIsTTY = Object.getOwnPropertyDescriptor(process.stdout, 'isTTY');
const stderrIsTTY = Object.getOwnPropertyDescriptor(process.stderr, 'isTTY');

common.mustCall(async () => {
  try {
    setIsTTY(process.stdout, true);
    assert.deepStrictEqual(await runWorker('stdout'), {
      hasColorDepth: true,
      hasColors: true,
      isTTY: true,
      result: styled,
    });

    setIsTTY(process.stderr, true);
    assert.deepStrictEqual(await runWorker('stderr'), {
      hasColorDepth: true,
      hasColors: true,
      isTTY: true,
      result: styled,
    });

    assert.deepStrictEqual(await runWorker('stdout', { stdout: true }), {
      hasColorDepth: false,
      hasColors: false,
      isTTY: undefined,
      result: plain,
    });

    assert.deepStrictEqual(await runWorker('stderr', { stderr: true }), {
      hasColorDepth: false,
      hasColors: false,
      isTTY: undefined,
      result: plain,
    });

    setIsTTY(process.stdout, false);
    assert.deepStrictEqual(await runWorker('stdout'), {
      hasColorDepth: false,
      hasColors: false,
      isTTY: undefined,
      result: plain,
    });
  } finally {
    restoreIsTTY(process.stdout, stdoutIsTTY);
    restoreIsTTY(process.stderr, stderrIsTTY);
  }
})();
