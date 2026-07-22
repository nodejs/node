import { spawnPromisified } from '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: experiemental warning for import.meta.resolve', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should not warn when caught', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      '--experimental-import-meta-resolve',
      path('es-modules/import-resolve-exports.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
