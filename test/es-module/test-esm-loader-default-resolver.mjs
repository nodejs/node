import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('default resolver', () => {
  // In these tests `byop` is an acronym for "bring your own protocol", and is the
  // protocol our byop-dummy-loader.mjs can load
  it('should accept foreign schemas without exception (e.g. byop://something/or-other)', async () => {
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/byop-dummy-loader.mjs'),
      '--input-type=module',
      '--eval',
      "import 'byop://1/index.mjs'",
    ]);
    assert.strictEqual(code, 0);
    assert.strictEqual(stdout.trim(), 'index.mjs!');
    assert.strictEqual(stderr, '');
  });

  it('should resolve foreign schemas by doing regular url absolutization', async () => {
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/byop-dummy-loader.mjs'),
      '--input-type=module',
      '--eval',
      "import 'byop://1/index2.mjs'",
    ]);
    assert.strictEqual(code, 0);
    assert.strictEqual(stdout.trim(), '42');
    assert.strictEqual(stderr, '');
  });

  // In this test, `byoe` is an acronym for "bring your own extension"
  it('should accept foreign extensions without exception (e.g. ..//something.byoe)', async () => {
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--experimental-loader',
      fixtures.fileURL('/es-module-loaders/byop-dummy-loader.mjs'),
      '--input-type=module',
      '--eval',
      "import 'byop://1/index.byoe'",
    ]);
    assert.strictEqual(code, 0);
    assert.strictEqual(stdout.trim(), 'index.byoe!');
    assert.strictEqual(stderr, '');
  });

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
