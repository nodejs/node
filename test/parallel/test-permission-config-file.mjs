import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { describe, it } from 'node:test';

describe('Permission model config file support', () => {
  it('should load filesystem read/write permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-fs-read-write.json');
    const readTestPath = fixtures.path('permission/fs-read-test.js');
    const writeTestPath = fixtures.path('permission/fs-write-test.js');

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        configPath,
        readTestPath,
      ]);
      assert.strictEqual(result.code, 0);
    }

    {
      const result = await spawnPromisified(process.execPath, [
        '--permission',
        '--experimental-config-file',
        configPath,
        writeTestPath,
      ]);
      assert.strictEqual(result.code, 0);
    }
  });

  it('should load child process and worker permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-child-worker.json');
    const childTestPath = fixtures.path('permission/child-process-test.js');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-fs-read=*',
      childTestPath,
    ]);
    assert.strictEqual(result.code, 0);
  });

  it('should load network and inspector permissions from config file', async () => {
    const configPath = fixtures.path('permission/config-net-inspector.json');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-fs-read=*',
      '-p',
      'process.permission.has("net") && process.permission.has("inspector")',
    ]);
    assert.match(result.stdout, /true/);
    assert.strictEqual(result.code, 0);
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

  it('should deny operations when permissions are not in config file', async () => {
    const configPath = fixtures.path('permission/config-fs-read-write.json');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-fs-read=*',
      '-p',
      'process.permission.has("child")',
    ]);
    assert.match(result.stdout, /false/);
    assert.strictEqual(result.code, 0);
  });

  it('should combine config file permissions with CLI flags', async () => {
    const configPath = fixtures.path('permission/config-fs-read-write.json');

    const result = await spawnPromisified(process.execPath, [
      '--permission',
      '--experimental-config-file',
      configPath,
      '--allow-child-process',
      '--allow-fs-read=*',
      '-p',
      'process.permission.has("child") && process.permission.has("fs.read")',
    ]);
    assert.match(result.stdout, /true/);
    assert.strictEqual(result.code, 0);
  });
});
