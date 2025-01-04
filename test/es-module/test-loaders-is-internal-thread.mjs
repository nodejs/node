import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('Worker threads do not spawn infinitely', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should not trigger an infinite loop when using a loader exports no recognized hooks', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('loader-is-internal-worker.js'),
      '--eval',
      'setTimeout(() => {},99)',
    ]);

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^true\r?\n$/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
