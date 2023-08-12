import { spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';
import { fileURLToPath } from 'node:url';

const __dirname = fileURLToPath(new URL('.', import.meta.url));
const validEnvFilePath = '../fixtures/dotenv/valid.env';
const relativePath = '../fixtures/dotenv/node-options.env';

describe('.env supports NODE_OPTIONS', () => {

  it('should have access to permission scope', async () => {
    const code = `
      require('assert')(process.permission.has('fs.read'));
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${relativePath}`, '--eval', code ], {
                                           cwd: __dirname
                                         });
    // Test NODE_NO_WARNINGS environment variable
    // `stderr` should not contain "ExperimentalWarning: Permission is an experimental feature" message
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('validate only read permissions are enabled', async () => {
    const code = `
      require('fs').writeFileSync(require('path').join(__dirname, 'should-not-write.txt'), 'hello', 'utf-8')
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${relativePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                         });
    assert.match(child.stderr, /Error: Access to this API has been restricted/);
    assert.match(child.stderr, /code: 'ERR_ACCESS_DENIED'/);
    assert.match(child.stderr, /permission: 'FileSystemWrite'/);
    assert.strictEqual(child.code, 1);
  });

  it('TZ environment variable', async () => {
    const code = `
      require('assert')(new Date().toString().includes('Hawaii'))
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${relativePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                         });
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

  it('should update UV_THREADPOOL_SIZE', async () => {
    const code = `
      require('assert').strictEqual(process.env.UV_THREADPOOL_SIZE, '5')
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${relativePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                         });
    assert.strictEqual(child.stderr, '');
    assert.strictEqual(child.code, 0);
  });

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
