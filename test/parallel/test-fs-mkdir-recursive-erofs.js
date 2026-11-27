'use strict';

// Test that mkdir with the recursive option reports EROFS when the target is
// on a read-only file system instead of turning it into ENOENT.
// Ref: https://github.com/nodejs/node/issues/47098

const common = require('../common');

if (common.isWindows)
  common.skip('no read-only file system to test against on Windows');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const probe = `node-test-erofs-${process.pid}`;

// Find a mounted read-only file system. Linux lists mount options in
// /proc/self/mounts (for example /sys in a container, or squashfs images);
// on macOS the system volume is read-only. Confirm the candidate with a plain
// mkdir, which must fail with EROFS rather than with a permission error.
function findReadOnlyRoot() {
  const candidates = [];
  if (common.isLinux) {
    let mounts;
    try {
      mounts = fs.readFileSync('/proc/self/mounts', 'utf8');
    } catch {
      return null;
    }
    for (const line of mounts.split('\n')) {
      const fields = line.split(' ');
      if (fields.length >= 4 && /(^|,)ro(,|$)/.test(fields[3])) {
        candidates.push(fields[1].replaceAll('\\040', ' '));
      }
    }
  } else if (common.isMacOS) {
    candidates.push('/');
  }

  for (const dir of candidates) {
    try {
      fs.mkdirSync(path.join(dir, probe));
    } catch (err) {
      if (err.code === 'EROFS') return dir;
      continue;
    }
    // Not read-only after all.
    fs.rmdirSync(path.join(dir, probe));
  }
  return null;
}

const root = findReadOnlyRoot();
if (root === null)
  common.skip('no read-only file system available');

const target = path.join(root, probe, 'child');
const expected = { code: 'EROFS', syscall: 'mkdir' };

assert.throws(() => fs.mkdirSync(target, { recursive: true }), expected);

fs.mkdir(target, { recursive: true }, common.mustCall((err) => {
  assert.strictEqual(err.code, 'EROFS');
  assert.strictEqual(err.syscall, 'mkdir');
  assert(err.path);
}));

assert.rejects(fs.promises.mkdir(target, { recursive: true }), expected)
  .then(common.mustCall());
