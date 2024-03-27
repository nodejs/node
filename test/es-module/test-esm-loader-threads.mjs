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
    strictEqual(stdout.split('\n').filter(line => line.startsWith('initialize')).length, 1);
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
