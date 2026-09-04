'use strict';

// This tests that a compile cache persistence failure does not leave the
// temporary file used to write it behind.

const common = require('../common');
if (common.isWindows)
  common.skip('no RLIMIT_FSIZE on Windows');
if (process.config.variables.node_shared)
  common.skip('SIGXFSZ signal handler not installed in shared library mode');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const cacheDir = tmpdir.resolve('compile_cache');
fs.writeFileSync(tmpdir.resolve('fixture.cjs'), 'module.exports = 42;\n');

const [cmd, opts] = common.escapePOSIXShell`ulimit -f 0 && "${process.execPath}" -e "require('./fixture.cjs')"`;
opts.env.NODE_COMPILE_CACHE = cacheDir;
opts.cwd = tmpdir.path;
const result = spawnSync('/bin/sh', ['-c', cmd], opts);
assert.strictEqual(result.status, 0, result.stderr.toString());

const subdirs = fs.readdirSync(cacheDir);
assert.strictEqual(subdirs.length, 1);
const leftover = fs.readdirSync(path.join(cacheDir, subdirs[0]));
assert.deepStrictEqual(leftover, []);
