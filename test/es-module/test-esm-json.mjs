import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

import secret from '../fixtures/experimental.json' assert { type: 'json' };


describe('ESM: importing JSON', () => {
  it('should load JSON', () => {
    assert.strictEqual(secret.ofLife, 42);
  });

  it('should print an experimental warning', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      fixtures.path('/es-modules/json-modules.mjs'),
    ]);

    assert.match(stderr, /ExperimentalWarning/);
    assert.match(stderr, /JSON modules/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
