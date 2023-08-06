'use strict';

require('../common');
const assert = require('node:assert');
const { spawnPromisified } = require('../common');
const { describe, it } = require('node:test');

const envFilePath = '../fixtures/dotenv/node-options.env';

describe('.env supports NODE_OPTIONS', () => {

  it('should have access to permission scope', async () => {
    const code = '"require(\'assert\').assert(process.permission.has(\'fs.read\'))\'"';
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${envFilePath}`, '--eval', code ], {
                                           cwd: __dirname
                                         });
    // Test NODE_NO_WARNINGS environment variable
    // `stderr` should not contain "ExperimentalWarning: Permission is an experimental feature" message
    assert.strictEqual(child.stderr.toString(), '');
    assert.strictEqual(child.code, 0);
  });

  it('validate only read permissions are enabled', async () => {
    const code = `
      "require('fs').writeFileSync(require('path').join(__dirname, 'should-not-write.txt'), 'hello', 'utf-8')"
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${envFilePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                           encoding: 'utf-8',
                                         });
    assert.strictEqual(child.code, 0);
  });

  it('TZ environment variable', async () => {
    const code = `
      "require('assert').assert(new Date().toString().includes('Hawaii'))"
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${envFilePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                           encoding: 'utf-8',
                                         });
    assert.strictEqual(child.code, 0);
  });

  it('should update UV_THREADPOOL_SIZE', async () => {
    const code = `
      "require('assert').assert.strictEqual(process.env.UV_THREADPOOL_SIZE, 5)"
    `.trim();
    const child = await spawnPromisified(process.execPath,
                                         [ `--env-file=${envFilePath}`, '--eval', code ], {
                                           cwd: __dirname,
                                           encoding: 'utf-8',
                                         });
    assert.strictEqual(child.code, 0);
  });
});
