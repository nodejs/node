import { isIBMi, isSunOS, isWindows, skip } from '../common/index.mjs';
import assert from 'node:assert';
import process, { setTitle } from 'node:process';
import { isMainThread } from 'node:worker_threads';
import { describe, it } from 'node:test';

// FIXME add sunos support
if (isSunOS || isIBMi || isWindows) {
  skip(`Unsupported platform [${process.platform}]`);
}

if (!isMainThread) {
  skip('Setting the process title from Workers is not supported');
}

describe('ESM named import setTitle', () => {
  it('sets process title through node:process', () => {
    const title = String(process.pid);

    setTitle(title);
    assert.strictEqual(process.title, title);

    assert.throws(() => setTitle(1), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });
});
