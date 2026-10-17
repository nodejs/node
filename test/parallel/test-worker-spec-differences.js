'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const path = require('path');

function workerURL(name) {
  return fixtures.fileURL('web-worker', name).href;
}

// Asserts that `worker` posts exactly one message and terminates it. A worker
// is kept alive by its message port, so it never exits on its own.
function expectMessage(worker, check) {
  worker.addEventListener('error', common.mustNotCall('worker failed'));
  worker.addEventListener('message', common.mustCall(({ data }) => {
    check(data);
    worker.terminate();
  }));
}

// Asserts that `worker` fails to run and fires an error event.
function expectError(worker, check) {
  worker.addEventListener('message', common.mustNotCall('worker succeeded'));
  worker.addEventListener('error', check);
}

process.on('uncaughtException', common.mustNotCall(
  'worker errors must not be reported on the main thread'));

// The only supported schemes are 'file:', 'blob:', and 'data:'
for (const unsupported of [
  'https://nodejs.org/worker.js',
  'http://localhost/worker.js',
  'about:blank',
  'node:worker_threads',
]) {
  assert.throws(() => new Worker(unsupported), { name: 'NotSupportedError' });
}

// Same as above, for `importScripts`
expectMessage(new Worker(workerURL('import-scheme.js')),
              common.mustCall((data) => assert.strictEqual(data, 'NetworkError')));

// Rejection events are emitted on the process
expectMessage(new Worker(workerURL('no-events.js')), common.mustCall((data) => {
  assert.deepStrictEqual(data.dispatched, []);
  assert.strictEqual(data.processEvents, 1);
}));

{
  // Calling close() immediately terminates the worker
  const worker = new Worker(workerURL('close.js'));
  worker.addEventListener('error', common.mustNotCall('worker failed'));
  worker.addEventListener('message', common.mustCall(({ data }) => {
    assert.strictEqual(data, 'before-close');
  }, 1));
}

{
  // Relative imports
  const target = fixtures.path('web-worker', 'relative.js');
  const relative =
    path.relative(process.cwd(), target).replaceAll(path.sep, '/');
  expectMessage(new Worker(relative.startsWith('.') ? relative : `./${relative}`),
                common.mustCall((data) => assert.strictEqual(data, 'relative')));
}

{
  // The classic type takes precedence over the file extension.
  expectMessage(new Worker(workerURL('classic.mjs'), { type: 'classic' }),
                common.mustCall((data) => assert.deepStrictEqual(data, {
                  binding: 'classic',
                  requireType: 'function',
                  thisIsGlobal: true,
                })));
}

{
  // The module type takes precedence over the file extension and preserves
  // the file URL as the base for relative imports.
  expectMessage(new Worker(workerURL('module.cjs'), { type: 'module' }),
                common.mustCall((data) => assert.deepStrictEqual(data, {
                  requireType: 'undefined',
                  thisIsUndefined: true,
                  value: 'module',
                })));
}

if (common.hasCrypto) {
  {
    // MIME validation must use the Blob's internal type, not its public getter.
    const blob = new Blob(['postMessage("unexpected")'], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const typeDescriptor = Object.getOwnPropertyDescriptor(Blob.prototype, 'type');
    Object.defineProperty(Blob.prototype, 'type', {
      configurable: true,
      get() { return 'text/javascript'; },
    });
    let worker;
    try {
      worker = new Worker(url);
    } finally {
      Object.defineProperty(Blob.prototype, 'type', typeDescriptor);
    }
    expectError(worker, common.mustCall(({ message }) => {
      assert.match(message, /^Failed to fetch the worker script:/);
      URL.revokeObjectURL(url);
    }));
  }

  {
    // Module source encoding must not call mutable Buffer prototype methods.
    const blob = new Blob(['postMessage("original")'], {
      type: 'text/javascript',
    });
    const url = URL.createObjectURL(blob);
    const bufferToString = Buffer.prototype.toString;
    Buffer.prototype.toString = common.mustNotCall();
    let worker;
    try {
      worker = new Worker(url, { type: 'module' });
    } finally {
      Buffer.prototype.toString = bufferToString;
    }
    expectMessage(worker, common.mustCall((data) => {
      assert.strictEqual(data, 'original');
      URL.revokeObjectURL(url);
    }));
  }
} else {
  common.printSkipMessage('skipping Blob tests due to lack of randomUUID()');
}
