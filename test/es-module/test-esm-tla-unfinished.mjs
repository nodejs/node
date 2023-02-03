import { spawnPromisified } from '../common/index.mjs';
import fixtures from '../common/fixtures.js';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


const commonArgs = [
  '--no-warnings',
  '--input-type=module',
  '--eval',
];

describe('ESM: unsettled and rejected promises', { concurrency: true }, () => {
  it('should exit for an unsettled TLA promise via --eval', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      ...commonArgs,
      'await new Promise(() => {})',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 13);
  });

  it('should throw for a rejected TLA promise via --eval', async () => {
    // Rejected TLA promise, --eval
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      ...commonArgs,
      'await Promise.reject(new Error("Xyz"))',
    ]);

    assert.match(stderr, /Error: Xyz/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });

  it('should exit for an unsettled TLA promise and respect explicit exit code via --eval', async () => {
    // Rejected TLA promise, --eval
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      ...commonArgs,
      'process.exitCode = 42;await new Promise(() => {})',
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 42);
  });

  it('should throw for a rejected TLA promise and ignore explicit exit code via --eval', async () => {
    // Rejected TLA promise, --eval
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      ...commonArgs,
      'process.exitCode = 42;await Promise.reject(new Error("Xyz"))',
    ]);

    assert.match(stderr, /Error: Xyz/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });

  it('should exit for an unsettled TLA promise via stdin', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/unresolved.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 13);
  });

  it('should throw for a rejected TLA promise via stdin', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/rejected.mjs'),
    ]);

    assert.match(stderr, /Error: Xyz/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });

  it('should exit for an unsettled TLA promise and respect explicit exit code via stdin', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/unresolved-withexitcode.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 42);
  });

  it('should throw for a rejected TLA promise and ignore explicit exit code via stdin', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/rejected-withexitcode.mjs'),
    ]);

    assert.match(stderr, /Error: Xyz/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });

  it('should exit successfully when calling `process.exit()` in `.mjs` file', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/process-exit.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 0);
  });

  it('should be unaffected by `process.exit()` in worker thread', async () => {
    const { code, stderr, stdout } = await spawnPromisified(execPath, [
      '--no-warnings',
      fixtures.path('es-modules/tla/unresolved-with-worker-process-exit.mjs'),
    ]);

    assert.strictEqual(stderr, '');
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 13);
  });
});
