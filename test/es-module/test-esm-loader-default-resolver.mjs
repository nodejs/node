import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('default resolver', () => {
  it('should accept foreign schemas without exception (e.g. uyyt://something/or-other', async () => {
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/uyyt-dummy-loader.mjs'),
      fixtures.path('/es-module-loaders/uyyt-dummy-loader-main.mjs'),
    ]);
    assert.strictEqual(code, 0);
    assert.strictEqual(stdout.trim(), 'index.mjs!');
    assert.strictEqual(stderr, '');
  });

  it('should resolve foreign schemas by doing regular url absolutization', async () => {
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/uyyt-dummy-loader.mjs'),
      fixtures.path('/es-module-loaders/uyyt-dummy-loader-main2.mjs'),
    ]);
    assert.strictEqual(code, 0);
    assert.strictEqual(stdout.trim(), '42');
    assert.strictEqual(stderr, '');
  });
});
