'use strict';

// Refs: https://github.com/nodejs/node/issues/58892

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = tmpdir.path;
fs.mkdirSync(path.join(dir, 'sub'));
fs.writeFileSync(path.join(dir, 'a.txt'), 'a');
fs.writeFileSync(path.join(dir, 'sub', 'b.txt'), 'b');

const expected = fs.readdirSync(dir, { recursive: true }).sort();

// Buffer path must not throw and must match the string-path result.
const actual = fs.readdirSync(Buffer.from(dir), { recursive: true }).sort();
assert.deepStrictEqual(actual, expected);

const bufResult = fs.readdirSync(Buffer.from(dir), {
  recursive: true,
  encoding: 'buffer',
});
assert.ok(bufResult.every((entry) => Buffer.isBuffer(entry)));
assert.deepStrictEqual(
  bufResult.map((entry) => entry.toString()).sort(),
  expected,
);

const expectedDirents = fs.readdirSync(dir, {
  recursive: true,
  withFileTypes: true,
});
const dirents = fs.readdirSync(Buffer.from(dir), {
  recursive: true,
  withFileTypes: true,
});
assert.strictEqual(dirents.length, expectedDirents.length);
assert.ok(dirents.every((dirent) => dirent instanceof fs.Dirent));
assert.deepStrictEqual(
  dirents.map((dirent) => ({
    name: dirent.name,
    parentPath: dirent.parentPath,
    isFile: dirent.isFile(),
    isDirectory: dirent.isDirectory(),
  })).sort((a, b) => (a.name < b.name ? -1 : 1)),
  expectedDirents.map((dirent) => ({
    name: dirent.name,
    parentPath: dirent.parentPath,
    isFile: dirent.isFile(),
    isDirectory: dirent.isDirectory(),
  })).sort((a, b) => (a.name < b.name ? -1 : 1)),
);
