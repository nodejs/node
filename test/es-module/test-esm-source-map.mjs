import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

describe('esm source-map', { concurrency: true }, () => {
  // Issue: https://github.com/nodejs/node/issues/51522

  it('should extract source map url in middle from esm', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--enable-source-maps',
      fixtures.path('source-map/extract-url/esm-url-in-middle.mjs'),
    ]);

    assert.strictEqual(stdout, '');
    assert.match(stderr, /index\.ts/);
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should extract source map url in middle from cjs imported by esm', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--enable-source-maps',
      '--input-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('source-map/extract-url/cjs-url-in-middle.js'))}`
    ]);

    assert.strictEqual(stdout, '');
    assert.match(stderr, /index\.ts/);
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should not extract source map url inside string from esm', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--enable-source-maps',
      fixtures.path('source-map/extract-url/esm-url-in-string.mjs'),
    ]);

    assert.strictEqual(stdout, '');
    assert.doesNotMatch(stderr, /index\.ts/);
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });

  it('should not extract source map url inside string from cjs imported by esm', async () => {
    const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--enable-source-maps',
      '--input-type=module',
      '--eval',
      `import ${JSON.stringify(fixtures.fileURL('source-map/extract-url/cjs-url-in-string.js'))}`
    ]);

    assert.strictEqual(stdout, '');
    assert.doesNotMatch(stderr, /index\.ts/);
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });
});