import { spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';
import { fileURLToPath } from 'node:url';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const validEnvFilePath = '../fixtures/dotenv/valid.env';
const relativePath = '../fixtures/dotenv/node-options.env';

describe('.env supports edge cases', () => {

  it('should use the last --env-file declaration', async () => {
    const code = `
      require('assert').strictEqual(process.env.BASIC, 'basic');
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${relativePath}`, `--env-file=${validEnvFilePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                         });
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should handle unexisting .env file', async () => {
    const code = `
      require('assert').strictEqual(1, 1)
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ '--env-file=.env', '--eval', code ], {
                                           cwd: __dirname,
                                         });
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

});
