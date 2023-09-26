import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it, test } from 'node:test';

import { mkdir, rm, writeFile } from 'node:fs/promises';
import * as tmpdir from '../common/tmpdir.js';

import secret from '../fixtures/experimental.json' assert { type: 'json' };

describe('ESM: importing JSON', () => {
  it('should load JSON', () => {
    assert.strictEqual(secret.ofLife, 42);
  });

  it('should print an experimental warning', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      fixtures.path('/es-modules/json-modules.mjs'),
    ]);

    assert.match(stderr, /ExperimentalWarning: Importing JSON modules/);
    assert.match(stderr, /ExperimentalWarning: Import assertions/);
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  test('should load different modules when the URL is different', async (t) => {
    const root = tmpdir.fileURL(`./test-esm-json-${Math.random()}/`);
    try {
      await mkdir(root, { recursive: true });

      await t.test('json', async () => {
        const url = new URL('./foo.json', root);
        await writeFile(url, JSON.stringify({ firstJson: true }));
        const firstJson = await import(`${url}?a=1`, {
          assert: { type: 'json' },
          });
        await writeFile(url, JSON.stringify({ firstJson: false }));
        const secondJson = await import(`${url}?a=2`, {
          assert: { type: 'json' },
          });

        assert.notDeepStrictEqual(firstJson, secondJson);
      });

      await t.test('js', async () => {
        const url = new URL('./foo.mjs', root);
        await writeFile(url, `
      export const firstJS = true
      `);
        const firstJS = await import(`${url}?a=1`);
        await writeFile(url, `
      export const firstJS = false
      `);
        const secondJS = await import(`${url}?a=2`);
        assert.notDeepStrictEqual(firstJS, secondJS);
      });
    } finally {
      await rm(root, { force: true, recursive: true });
    }
  });
});
