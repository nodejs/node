// Flags: --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const { realpathCacheKey } = require('internal/fs/utils');
const path = require('path');
const tmpdir = require('../common/tmpdir');

if (process.argv[2] === 'child') {
  if (process.argv[4] === 'callback') {
    fs.realpath(process.argv[3], common.mustSucceed((result) => {
      process.stdout.write(result);
    }));
  } else {
    process.stdout.write(fs.realpathSync(process.argv[3]));
  }
} else {
  if (!common.canCreateSymLink())
    common.skip('insufficient privileges');

  tmpdir.refresh();

  // Reproduce a symlink target containing `..` after another symlink:
  //   c -> a/b/c
  //   d -> c/../d -> a/b/d
  // The cache fixture also creates a separate d to ensure that c/../d stays
  // distinct from the lexically normalized path d.
  const target = tmpdir.resolve('a', 'b', 'd');
  fs.mkdirSync(tmpdir.resolve('a', 'b', 'c'), { recursive: true });
  fs.mkdirSync(target);
  fs.symlinkSync(path.join('a', 'b', 'c'), tmpdir.resolve('c'), 'dir');
  fs.symlinkSync(`c${path.sep}..${path.sep}d`, tmpdir.resolve('d'), 'dir');

  const cacheRoot = tmpdir.resolve('cache');
  const cacheTarget = path.join(cacheRoot, 'a', 'b', 'd');
  const cachePlain = path.join(cacheRoot, 'd');
  fs.mkdirSync(path.join(cacheRoot, 'a', 'b', 'c'), { recursive: true });
  fs.mkdirSync(cacheTarget);
  fs.mkdirSync(cachePlain);
  fs.symlinkSync(path.join('a', 'b', 'c'), path.join(cacheRoot, 'c'), 'dir');
  const cacheDotted = `${cacheRoot}${path.sep}c${path.sep}..${path.sep}d`;
  const cacheExpected = fs.realpathSync(cacheTarget);
  const plainExpected = fs.realpathSync(cachePlain);

  function assertCacheIsolation(first, firstExpected, second, secondExpected) {
    const cache = new Map();
    const options = { [realpathCacheKey]: cache };
    assert.strictEqual(fs.realpathSync(first, options), firstExpected);
    assert.strictEqual(fs.realpathSync(second, options), secondExpected);
  }

  assertCacheIsolation(cacheDotted, cacheExpected, cachePlain, plainExpected);
  assertCacheIsolation(cachePlain, plainExpected, cacheDotted, cacheExpected);

  // Run realpathSync in a child because the regression does not return.
  const result = spawnSync(process.execPath, [
    '--expose-internals',
    __filename,
    'child',
    tmpdir.resolve('d'),
  ], {
    encoding: 'utf8',
    timeout: common.platformTimeout(5000),
  });

  assert.ifError(result.error);
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stdout, target);

  const directInput = `c${path.sep}..${path.sep}d`;
  const directResult = spawnSync(process.execPath, [
    '--expose-internals',
    __filename,
    'child',
    directInput,
  ], {
    encoding: 'utf8',
    cwd: tmpdir.path,
    timeout: common.platformTimeout(5000),
  });

  assert.ifError(directResult.error);
  assert.strictEqual(directResult.status, 0, directResult.stderr);
  assert.strictEqual(directResult.stdout, target);

  const callbackResult = spawnSync(process.execPath, [
    '--expose-internals',
    __filename,
    'child',
    tmpdir.resolve('d'),
    'callback',
  ], {
    encoding: 'utf8',
    timeout: common.platformTimeout(5000),
  });

  assert.ifError(callbackResult.error);
  assert.strictEqual(callbackResult.status, 0, callbackResult.stderr);
  assert.strictEqual(callbackResult.stdout, target);
}
