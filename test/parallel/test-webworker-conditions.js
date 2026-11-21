// Flags: --experimental-web-worker --conditions=custom-condition
'use strict';

// The `worker` package condition applies only within Web Worker threads,
// not on the main thread or in `worker_threads` threads.

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { once } = require('node:events');
const { Worker: ThreadWorker } = require('node:worker_threads');

const base = fixtures.fileURL('es-modules/worker-condition/');
const pkg = new URL('node_modules/pkg/', base);

function runWebWorker(url, options) {
  const worker = new Worker(url, options);
  worker.onerror = common.mustNotCall('worker failed');
  return new Promise((resolve) => {
    worker.onmessage = common.mustCall(({ data }) => {
      worker.terminate();
      resolve(data);
    });
  });
}

(async () => {
  // Main thread: no worker condition.
  assert.deepStrictEqual((await import(new URL('main.mjs', base))).default, {
    import: 'default',
    custom: 'custom',
    require: 'default',
    resolve: new URL('default.mjs', pkg).href,
    workerOnly: 'ERR_PACKAGE_PATH_NOT_EXPORTED',
  });

  // Module Web Worker: entry point dependencies, dynamic import,
  // import.meta.resolve, require, nested threads.
  assert.deepStrictEqual(
    await runWebWorker(new URL('module-worker.mjs', base), { type: 'module' }),
    {
      staticImport: 'worker',
      dynamicImport: 'worker',
      custom: 'custom-worker',
      workerOnly: 'worker',
      resolve: new URL('worker.mjs', pkg).href,
      require: 'worker',
      // worker_threads created from a Web Worker revert to no worker condition.
      thread: {
        import: 'default',
        custom: 'custom',
        require: 'default',
        resolve: new URL('default.mjs', pkg).href,
      },
      // Nested Web Workers retain it.
      nested: 'worker',
    });

  // Classic Web Worker.
  assert.deepStrictEqual(
    await runWebWorker(new URL('classic-worker.js', base)),
    {
      require: 'worker',
      import: 'worker',
      resolve: fixtures.path('es-modules/worker-condition/node_modules/pkg/worker.cjs'),
    });

  // Web Worker created from a worker_threads thread.
  const thread = new ThreadWorker(new URL('thread-spawns-webworker.mjs', base));
  const [threadResult] = await once(thread, 'message');
  assert.deepStrictEqual(threadResult, { thread: 'default', webWorker: 'worker' });

  // Async module customization hooks registered from a Web Worker.
  assert.strictEqual(
    await runWebWorker(new URL('hooks-worker.mjs', base), { type: 'module' }),
    'worker');
})().then(common.mustCall());
