// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-side-effect.mjs --require ./test/fixtures/es-module-loaders/loader-side-effect-require-preload.js
import { allowGlobals, mustCall } from '../common/index.mjs';
import assert from 'assert';
import { fileURLToPath } from 'url';
import { Worker, isMainThread, parentPort } from 'worker_threads';

/* global implicitGlobalProperty */
assert.strictEqual(globalThis.implicitGlobalProperty, 42);
allowGlobals(implicitGlobalProperty);

/* global implicitGlobalConst */
assert.strictEqual(implicitGlobalConst, 42 * 42);
allowGlobals(implicitGlobalConst);

/* global explicitGlobalProperty */
assert.strictEqual(globalThis.explicitGlobalProperty, 42 * 42 * 42);
allowGlobals(explicitGlobalProperty);

/* global preloadOrder */
assert.deepStrictEqual(globalThis.preloadOrder, ['--require', 'loader']);
allowGlobals(preloadOrder);

if (isMainThread) {
  const worker = new Worker(fileURLToPath(import.meta.url));
  const promise = new Promise((resolve, reject) => {
    worker.on('message', resolve);
    worker.on('error', reject);
  });
  promise.then(mustCall());
} else {
  parentPort.postMessage('worker done');
}
