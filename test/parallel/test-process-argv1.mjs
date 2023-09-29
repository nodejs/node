import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual } from 'node:assert';
import { describe, it } from 'node:test';
import { dirname } from 'node:path';

describe('process.argv1', { concurrency: true }, () => {
  it('should be the string for the entry point before the entry was resolved into an absolute path', async () => {
    const fixtureRelativePath = './process-argv1.js';
    const fixtureAbsolutePath = fixtures.path(fixtureRelativePath);

    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [fixtureRelativePath], {
      cwd: dirname(fixtureAbsolutePath),
    });

    strictEqual(stderr, '');
    deepStrictEqual(stdout.split('\n'), [
      fixtureRelativePath,
      fixtureAbsolutePath,
      '',
    ]);
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
