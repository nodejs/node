'use strict';

// Tests that drop() only revokes the exact resource that was explicitly granted.

const { spawnSync } = require('child_process');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  process.exit(0);
}

require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const dir = path.join(tmpdir.path, 'granted-dir');
fs.mkdirSync(dir);
fs.writeFileSync(path.join(dir, 'item1.txt'), 'aaa');
fs.writeFileSync(path.join(dir, 'item2.txt'), 'bbb');

// Grant a directory, try to drop a file inside it - should be a no-op
{
  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${dir}`,
    '-e',
    `
      const assert = require('assert');
      const fs = require('fs');
      const dir = ${JSON.stringify(dir)};

      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));

      process.permission.drop('fs.read', dir + '/item1.txt');

      // Still readable — drop of a file inside a granted dir is a no-op
      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));
      assert.ok(process.permission.has('fs.read', dir + '/item2.txt'));
    `,
  ]);
  if (child.status !== 0) {
    console.error('Case 1 stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0, 'Case 1 failed');
}

// Grant a directory, drop the same directory - should revoke all access
{
  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${dir}`,
    '-e',
    `
      const assert = require('assert');
      const dir = ${JSON.stringify(dir)};

      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));

      process.permission.drop('fs.read', dir);

      assert.ok(!process.permission.has('fs.read', dir + '/item1.txt'));
      assert.ok(!process.permission.has('fs.read', dir + '/item2.txt'));
    `,
  ]);
  if (child.status !== 0) {
    console.error('Case 2 stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0, 'Case 2 failed');
}

// Grant two directories, drop one - the other remains accessible
{
  const dir2 = path.join(tmpdir.path, 'other-dir');
  fs.mkdirSync(dir2);
  fs.writeFileSync(path.join(dir2, 'other.txt'), 'ccc');

  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${dir}`,
    `--allow-fs-read=${dir2}`,
    '-e',
    `
      const assert = require('assert');
      const fs = require('fs');
      const dir = ${JSON.stringify(dir)};
      const dir2 = ${JSON.stringify(dir2)};

      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));
      assert.ok(process.permission.has('fs.read', dir2 + '/other.txt'));

      process.permission.drop('fs.read', dir);

      assert.ok(!process.permission.has('fs.read', dir + '/item1.txt'));
      assert.ok(process.permission.has('fs.read', dir2 + '/other.txt'));
      assert.strictEqual(
        fs.readFileSync(dir2 + '/other.txt', 'utf8'),
        'ccc'
      );
    `,
  ]);
  if (child.status !== 0) {
    console.error('Case 3 stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0, 'Case 3 failed');
}

// Grant a directory and a file inside it separately, drop the file
// the directory grant still covers everything including that file
{
  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${dir}`,
    `--allow-fs-read=${path.join(dir, 'item1.txt')}`,
    '-e',
    `
      const assert = require('assert');
      const path = require('path');
      const dir = ${JSON.stringify(dir)};

      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));

      process.permission.drop('fs.read', path.join(dir, 'item1.txt'));

      // Still readable because the directory grant covers it
      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));
    `,
  ]);
  if (child.status !== 0) {
    console.error('Case 4 stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0, 'Case 4 failed');
}

// Drop entire scope without reference - revokes everything
{
  const child = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${dir}`,
    '-e',
    `
      const assert = require('assert');
      const dir = ${JSON.stringify(dir)};

      assert.ok(process.permission.has('fs.read', dir + '/item1.txt'));

      process.permission.drop('fs.read');

      assert.ok(!process.permission.has('fs.read', dir + '/item1.txt'));
      assert.ok(!process.permission.has('fs.read'));
    `,
  ]);
  if (child.status !== 0) {
    console.error('Case 5 stderr:', child.stderr?.toString());
  }
  assert.strictEqual(child.status, 0, 'Case 5 failed');
}
