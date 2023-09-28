import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { strictEqual } from 'node:assert';
import { describe, it } from 'node:test';
import { dirname } from 'node:path';

describe('process.argv1', { concurrency: true }, () => {
  it('should be the string for the entry point before the entry was resolved into an absolute path', async () => {
    const fixtureAbsolutePath = fixtures.path('process-argv1.js');
    const fixtureRelativePath = './process-argv1.js';
    const { code, signal, stdout, stderr } = await spawnPromisified(process.execPath, [fixtureRelativePath], {
      cwd: dirname(fixtureAbsolutePath),
    });

    strictEqual(stderr, '');
    strictEqual(stdout, `${fixtureRelativePath}\n${fixtureAbsolutePath}\n`);
    strictEqual(code, 0);
    strictEqual(signal, null);
  });
});
