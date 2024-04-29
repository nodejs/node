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
    // Calls to resolve/load:
    // 1x  main script: test/fixtures/es-module-loaders/workers-spawned.mjs
    // 3x worker_threads
    //    => 1x test/fixtures/es-module-loaders/worker-log.mjs
    //       2x test/fixtures/es-module-loaders/worker-log-again.mjs => once per worker-log.mjs Worker instance
    // 2x test/fixtures/es-module-loaders/worker-log.mjs => once per worker-log.mjs Worker instance
    // 4x test/fixtures/es-module-loaders/worker-log-again.mjs => 2x for each worker-log
    // 6x module-named-exports.mjs => 2x worker-log.mjs + 4x worker-log-again.mjs
    // ===========================
    // 16 calls to resolve + 16 calls to load hook for the registered custom loader
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('hooked resolve')).length, 16);
    strictEqual(stdout.split('\n').filter((line) => line.startsWith('hooked load')).length, 16);
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
