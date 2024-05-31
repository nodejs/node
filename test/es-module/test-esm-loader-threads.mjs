import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('off-thread hooks', { concurrency: true }, () => {
  it('uses only one hooks thread to support multiple application threads', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--import',
      `data:text/javascript,${encodeURIComponent(`
        import { register } from 'node:module';
        register(${JSON.stringify(fixtures.fileURL('es-module-loaders/hooks-log.mjs'))});
      `)}`,
      fixtures.path('es-module-loaders/workers-spawned.mjs'),
    ]);


    strictEqual(stderr, '');
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('initialize')).length, 1);
    strictEqual(stdout.split('\n').filter((line) => line === 'foo').length, 2);
    strictEqual(stdout.split('\n').filter((line) => line === 'bar').length, 4);

    //
    // Calls to resolve/load:
    //
    // 1x  test/fixtures/es-module-loaders/workers-spawned.mjs
    //   1x worker_threads
    //     2x (1 per each worker spawned in workers-spawned.mjs)
    //       1x  data:text/javascript
    //         1x node:module
    //         1x ./worker-log.mjs
    //           1x worker_threads
    //           1x ./module-named-exports.mjs
    //       Total: 4
    //     4x (2x per each worker spawned in ./worker-log.mjs)
    //       1x  data:text/javascript
    //       1x node:module
    //         1x ./module-named-exports.mjs
    //         1x ./worker-log-again.mjs
    //       Total: 4
    // --------------------------------------------------------
    // Sum: 1 + 1 + 2 * 5 + 4 * 4 = 28
    //
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('hooked resolve')).length, 28);
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('hooked load')).length, 28);
    strictEqual(code, 0);
    strictEqual(signal, null);
  });

  it('propagates the exit code from worker thread import exiting from resolve hook', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--import',
      `data:text/javascript,${encodeURIComponent(`
        import { register } from 'node:module';
        register(${JSON.stringify(fixtures.fileURL('es-module-loaders/hooks-exit-worker.mjs'))});
      `)}`,
      fixtures.path('es-module-loaders/worker-log-fail-worker-resolve.mjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('resolve process-exit-module-resolve')).length, 1);
    strictEqual(code, 42);
    strictEqual(signal, null);
  });

  it('propagates the exit code from worker thread import exiting from load hook', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--import',
      `data:text/javascript,${encodeURIComponent(`
        import { register } from 'node:module';
        register(${JSON.stringify(fixtures.fileURL('es-module-loaders/hooks-exit-worker.mjs'))});
      `)}`,
      fixtures.path('es-module-loaders/worker-log-fail-worker-load.mjs'),
    ]);

    strictEqual(stderr, '');
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('resolve process-exit-module-load')).length, 1);
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('load process-exit-on-load:///')).length, 1);
    strictEqual(code, 43);
    strictEqual(signal, null);
  });

});
