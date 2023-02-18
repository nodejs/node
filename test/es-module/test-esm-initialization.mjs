import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: ensure initialization happens only once', { concurrency: true }, () => {
  it(async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--loader',
      fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
      '--no-warnings',
      fixtures.path('es-modules', 'runmain.mjs'),
    ]);

    // Length minus 1 because the first match is the needle.
    const resolveHookRunCount = (stdout.match(/resolve passthru/g)?.length ?? 0) - 1;

    assert.strictEqual(stderr, '');
    /**
     * resolveHookRunCount = 2:
     * 1. fixtures/…/runmain.mjs
     * 2. node:module (imported by fixtures/…/runmain.mjs)
     */
    assert.strictEqual(resolveHookRunCount, 2);
    assert.strictEqual(code, 0);
  });
});
