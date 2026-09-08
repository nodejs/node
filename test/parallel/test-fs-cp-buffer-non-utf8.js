'use strict';

// Refs: https://github.com/nodejs/node/issues/58634
// With Buffer paths, fs.cpSync() copies files whose names are not valid UTF-8
// (which are permitted on POSIX) without mangling them.

const common = require('../common');

if (!common.isLinux) {
  common.skip('non-UTF-8 file names are only valid on Linux');
}

const assert = require('assert');
const { join, sep } = require('path');
const {
  cpSync, mkdirSync, writeFileSync, readFileSync, existsSync,
} = require('fs');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const src = Buffer.from(join(tmpdir.path, 'a'));
const dest = Buffer.from(join(tmpdir.path, 'b'));
mkdirSync(src, { recursive: true });

// Shift-JIS encoding of こんにちは世界 ("Hello, World"); not valid UTF-8.
const name = Buffer.from([
  0x82, 0xB1, 0x82, 0xF1, 0x82, 0xC9, 0x82,
  0xBF, 0x82, 0xCD, 0x90, 0x6C, 0x8C, 0x8E,
]);
const sepBuf = Buffer.from(sep);
const srcFile = Buffer.concat([src, sepBuf, name]);
writeFileSync(srcFile, 'content');

cpSync(src, dest, { recursive: true });

const destFile = Buffer.concat([dest, sepBuf, name]);
assert.ok(existsSync(destFile));
assert.strictEqual(readFileSync(destFile, 'utf8'), 'content');
