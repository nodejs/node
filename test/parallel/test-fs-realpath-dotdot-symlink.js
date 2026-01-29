'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

const common = require('../common');
const tmpdir = require('../common/tmpdir');

if (common.isWindows) {
  common.skip('symlink semantics differ on Windows');
}

tmpdir.refresh();

const cwd = path.join(tmpdir.path, 'test');
fs.mkdirSync(path.join(cwd, 'a', 'b', 'c'), { recursive: true });
fs.mkdirSync(path.join(cwd, 'a', 'b', 'd'), { recursive: true });

process.chdir(cwd);

// ln -s a/b/c c
fs.symlinkSync('a/b/c', 'c', 'dir');
// ln -s "c/../d" d
fs.symlinkSync('c/../d', 'd', 'dir');

const expected = path.resolve(path.join(cwd, 'a', 'b', 'd'));

// sync
assert.strictEqual(fs.realpathSync('d'), expected);
