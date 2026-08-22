'use strict';

// Flags: --expose-internals

const common = require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');
const { mkdirSync } = require('fs');
const path = require('path');
const { join, sep } = path;
const tmpdir = require('../common/tmpdir');
const {
  dirnamePath,
  isPathRoot,
  isSrcSubdir,
  joinPath,
  maybeDecodePath,
  pathEquals,
  resolveLinkTarget,
  resolvePath,
} = require('internal/fs/cp/path');

tmpdir.refresh();

const paths = [
  '',
  '.',
  '..',
  '/',
  '//',
  '///',
  'a',
  'a/',
  '/a',
  '//a',
  'a/b',
  '/a/b',
  'a//b',
  'a/./b',
  'a/../b',
  '../a',
  '../../a',
  '/a/../b',
  '/..',
  '/../../a',
];

for (const value of paths) {
  const buffer = Buffer.from(value);
  assert.strictEqual(dirnamePath(buffer).toString(), path.dirname(value));
  assert.strictEqual(resolvePath(buffer).toString(), path.resolve(value));
  assert.strictEqual(isPathRoot(buffer), path.parse(path.resolve(value)).root === path.resolve(value));
}

for (const parent of paths) {
  for (const name of ['x', 'xy', '.x', '..x']) {
    assert.strictEqual(
      joinPath(Buffer.from(parent), Buffer.from(name)).toString(),
      path.join(parent, name),
    );
  }
}

for (const src of paths) {
  for (const dest of paths) {
    const srcParts = path.resolve(src).split(path.sep).filter(Boolean);
    const destParts = path.resolve(dest).split(path.sep).filter(Boolean);
    const expected = srcParts.every((part, index) => destParts[index] === part);
    assert.strictEqual(
      isSrcSubdir(Buffer.from(src), Buffer.from(dest)),
      expected,
    );
  }
}

assert(pathEquals('abc', Buffer.from('abc')));
assert.strictEqual(maybeDecodePath(Buffer.from('abc'), true), 'abc');

if (!common.isWindows) {
  const parent = Buffer.from('/tmp/parent');
  const name = Buffer.from([0x66, 0x80, 0x6f]);
  const child = Buffer.concat([parent, Buffer.from('/'), name]);
  const grandchild = Buffer.concat([child, Buffer.from('/child')]);

  assert(joinPath(parent, name).equals(child));
  assert(dirnamePath(child).equals(parent));
  assert(isSrcSubdir(child, grandchild));
  assert(!isSrcSubdir(grandchild, child));
  assert.strictEqual(maybeDecodePath(name, true), name);
  assert(resolveLinkTarget(parent, name).equals(child));
}

if (!common.isWindows) {
  const absoluteStringTarget = '/tmp//target/../file';
  const absoluteBufferTarget = Buffer.from('/tmp//opaque/../target');
  const invalidParent = Buffer.from([0x2f, 0x74, 0x6d, 0x70, 0x2f, 0xff]);

  assert.strictEqual(
    resolveLinkTarget('/source', absoluteStringTarget),
    absoluteStringTarget,
  );
  assert(
    resolveLinkTarget(invalidParent, absoluteBufferTarget)
      .equals(absoluteBufferTarget),
  );
}


if (!common.isWindows) {
  const originalCwd = process.cwd();
  const unicodeCwd = join(tmpdir.path, 'unicode-\u03c0');
  mkdirSync(unicodeCwd, { recursive: true });
  process.chdir(unicodeCwd);
  try {
    assert(
      resolvePath(Buffer.from('child'))
        .equals(Buffer.from(join(unicodeCwd, 'child'))),
    );
    const invalidName = Buffer.from([0x63, 0x68, 0xff, 0x69, 0x6c, 0x64]);
    assert(
      joinPath(unicodeCwd, invalidName)
        .equals(Buffer.concat([
          Buffer.from(unicodeCwd),
          Buffer.from(sep),
          invalidName,
        ])),
    );
  } finally {
    process.chdir(originalCwd);
  }
}
