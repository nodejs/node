import { spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { it } from 'node:test';

it('should print the timing information for cjs', async () => {
  process.env.NODE_DEBUG = 'timing_module_cjs';
  const result = await spawnPromisified(execPath, ['--eval', 'require("url");'], {
    env: {
      NODE_DEBUG: 'timing_module_cjs',
    },
  });

  assert.strictEqual(result.code, 0);
  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.stdout, '');

  const firstLine = result.stderr.split('\n').find((line) => line.includes('[url]'));

  assert.notStrictEqual(firstLine, undefined);
  assert.ok(firstLine.includes('TIMING_MODULE_CJS'), `Not found TIMING_MODULE_CJS on ${firstLine}`);
  assert.ok(firstLine.includes('[url]:'), `Not found [url]: on ${firstLine}`);
  assert.ok(firstLine.endsWith('ms'), `Not found ms on ${firstLine}`);
});
