import { spawnPromisified } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import { mkdir, writeFile } from 'node:fs/promises';
import * as path from 'node:path';
import { execPath } from 'node:process';
import { describe, it, before } from 'node:test';

describe('ESM in main field', { concurrency: !process.env.TEST_PARALLEL }, () => {
  before(() => tmpdir.refresh());

  it('should handle fully-specified relative path without any warning', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      main: './index.js',
      type: 'module',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^Hello World!\r?\n$/);
    assert.strictEqual(code, 0);
  });
  it('should handle fully-specified absolute path without any warning', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      main: path.join(pkgPath, './index.js'),
      type: 'module',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^Hello World!\r?\n$/);
    assert.strictEqual(code, 0);
  });

  it('should throw when "main" and "exports" are missing', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      type: 'module',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.match(stderr, /ERR_INVALID_PACKAGE_CONFIG/);
    assert.match(stderr, /Default "index" lookups for the main are not supported for ES modules/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });
  it('should throw when "main" is falsy', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      type: 'module',
      main: '',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.match(stderr, /ERR_INVALID_PACKAGE_CONFIG/);
    assert.match(stderr, /Default "index" lookups for the main are not supported for ES modules/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });
  it('should throw when "main" is a relative path without extension', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      main: 'index',
      type: 'module',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.match(stderr, /ERR_INVALID_PACKAGE_CONFIG/);
    assert.match(stderr, /Automatic extension resolution of the "main" field is not supported/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });
  it('should throw when "main" is an absolute path without extension', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({
      main: pkgPath + 'index',
      type: 'module',
    }));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.match(stderr, /ERR_INVALID_PACKAGE_CONFIG/);
    assert.match(stderr, /Automatic extension resolution of the "main" field is not supported/);
    assert.strictEqual(stdout, '');
    assert.strictEqual(code, 1);
  });
  it('should still resolve CommonJS packages via index lookup', async () => {
    const cwd = tmpdir.resolve(Math.random().toString());
    const pkgPath = path.join(cwd, './node_modules/pkg/');
    await mkdir(pkgPath, { recursive: true });
    await writeFile(path.join(pkgPath, './index.js'), 'console.log("Hello World!")');
    await writeFile(path.join(pkgPath, './package.json'), JSON.stringify({}));
    const { code, stdout, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval', 'import "pkg"',
    ], { cwd });

    assert.strictEqual(stderr, '');
    assert.match(stdout, /^Hello World!\r?\n$/);
    assert.strictEqual(code, 0);
  });
});
