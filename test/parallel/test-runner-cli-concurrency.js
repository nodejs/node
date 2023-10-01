'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { deepStrictEqual, strictEqual } = require('node:assert');
const { spawnSync } = require('node:child_process');
const { readdirSync, writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { beforeEach, test } = require('node:test');

function createTestFile(name) {
  writeFileSync(join(tmpdir.path, name), `
    const fs = require('node:fs');

    fs.unlinkSync(__filename);
    setTimeout(() => {}, 1_000_000_000);
  `);
}

beforeEach(() => {
  tmpdir.refresh();
  createTestFile('test-1.js');
  createTestFile('test-2.js');
});

test('concurrency of one', () => {
  const cp = spawnSync(process.execPath, ['--test', '--test-concurrency=1'], {
    cwd: tmpdir.path,
    timeout: common.platformTimeout(1000),
  });

  strictEqual(cp.stderr.toString(), '');
  strictEqual(cp.error.code, 'ETIMEDOUT');
  deepStrictEqual(readdirSync(tmpdir.path), ['test-2.js']);
});

test('concurrency of two', () => {
  const cp = spawnSync(process.execPath, ['--test', '--test-concurrency=2'], {
    cwd: tmpdir.path,
    timeout: common.platformTimeout(1000),
  });

  strictEqual(cp.stderr.toString(), '');
  strictEqual(cp.error.code, 'ETIMEDOUT');
  deepStrictEqual(readdirSync(tmpdir.path), []);
});
