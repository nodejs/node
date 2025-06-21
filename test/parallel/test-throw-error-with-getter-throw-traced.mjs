import { spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('--trace-uncaught', () => {
  it('prints a trace on process exit for uncaught errors', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      '--trace-uncaught',
      '--eval',
      `throw {
        get stack() {
          throw new Error('weird throw but ok');
        },
        get name() {
          throw new Error('weird throw but ok');
        },
      };`,
    ]);

    assert.match(stderr, /^Thrown at:$/m);
    assert.match(stderr, /^ {4}at \[eval\]:1:1$/m);
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });
});
