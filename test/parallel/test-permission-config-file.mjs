import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';

describe('Permission model config file support', () => {
  it('should load filesystem read/write permissions from config file', async () => {
    const readWriteConfigPath = fixtures.path('permission/config-fs-read-write.json');
    const readOnlyConfigPath = fixtures.path('permission/config-fs-read-only.json');
    const readTestPath = fixtures.path('permission/fs-read-test.js');
    const writeTestPath = fixtures.path('permission/fs-write-test.js');

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        readOnlyConfigPath,
        readTestPath,
      ]);
      assert.strictEqual(result.code, 0);
    }

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        readWriteConfigPath,
        writeTestPath,
      ]);
      assert.strictEqual(result.code, 0);
    }

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        readOnlyConfigPath,
        writeTestPath,
      ]);
      assert.strictEqual(result.code, 1);
      assert.match(result.stderr, /Access to this API has been restricted\. Use --allow-fs-write to manage permissions/);
    }
  });

  it('should load child process and worker permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-child-worker.json');
    const readOnlyConfigPath = fixtures.path('permission/config-fs-read-only.json');
    const childTestPath = fixtures.path('permission/child-process-test.js');

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        configPath,
        childTestPath,
      ]);
      assert.strictEqual(result.code, 0);
    }

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        readOnlyConfigPath,
        childTestPath,
      ]);
      assert.strictEqual(result.code, 1, result.stderr);
      assert.match(result.stderr, /Access to this API has been restricted\. Use --allow-child-process to manage permissions/);
    }
  });

  it('should load network and inspector permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-net-inspector.json');
    const readOnlyConfigPath = fixtures.path('permission/config-fs-read-only.json');

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        configPath,
        '-p',
        'process.permission.has("net") && process.permission.has("inspector")',
      ]);
      assert.match(result.stdout, /true/);
      assert.strictEqual(result.code, 0);
    }

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        readOnlyConfigPath,
        '-p',
        'process.permission.has("net") + process.permission.has("inspector")',
      ]);
      assert.match(result.stdout, /0/);
      assert.strictEqual(result.code, 0);
    }
  });

  it('should load addons and wasi permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-addons-wasi.json');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-fs-read=*',
      '-p',
      'process.permission.has("addon") && process.permission.has("wasi")',
    ]);
    assert.match(result.stdout, /true/);
    assert.strictEqual(result.code, 0);
  });

  it('should combine config file permissions with CLI flags', async () => {
    const configPath = fixtures.path('permission/config-fs-read-only.json');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-child-process',
      '--allow-fs-write=*',
      '-p',
      'process.permission.has("child") && process.permission.has("fs.read") && process.permission.has("fs.write")',
    ]);
    assert.match(result.stdout, /true/);
    assert.strictEqual(result.code, 0);
  });
});
