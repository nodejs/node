import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: nonexistent loader', () => {
  it('should throw', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--experimental-loader',
      'i-dont-exist',
      fixtures.path('print-error-message.js'),
    ]);

    assert.notStrictEqual(code, 0);

    // Error [ERR_MODULE_NOT_FOUND]: Cannot find package 'i-dont-exist' imported from
    assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
    assert.match(stderr, /'i-dont-exist'/);

    assert.ok(!stderr.includes('Bad command or file name'));
  });
});
