import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('default resolver', () => {
  it('should identify the parent module of an invalid URL host in import specifier', async () => {
    if (process.platform === 'win32') return;

    const { code, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules', 'invalid-posix-host.mjs'),
    ]);

    assert.match(stderr, /ERR_INVALID_FILE_URL_HOST/);
    assert.match(stderr, /file:\/\/hmm\.js/);
    assert.match(stderr, /invalid-posix-host\.mjs/);
    assert.strictEqual(code, 1);
  });
});
