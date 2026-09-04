// This tests that cpSync dereferences symlinks found inside the copied tree,
// not only a symlink passed as src.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, lstatSync, mkdirSync, readFileSync, symlinkSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
const target = nextdir();
const dest = nextdir();

mkdirSync(src, { recursive: true });
mkdirSync(join(target, 'dir'), { recursive: true });
writeFileSync(join(target, 'file.txt'), 'file', 'utf8');
writeFileSync(join(target, 'dir', 'nested.txt'), 'nested', 'utf8');
// Relative, as in the report: the link is resolved against its own directory.
symlinkSync(join('..', basename(target), 'file.txt'), join(src, 'link-to-file'));
symlinkSync(join(target, 'dir'), join(src, 'link-to-dir'), 'dir');

cpSync(src, dest, mustNotMutateObjectDeep({ dereference: true, recursive: true }));

assert(!lstatSync(join(dest, 'link-to-file')).isSymbolicLink());
assert.strictEqual(readFileSync(join(dest, 'link-to-file'), 'utf8'), 'file');

assert(!lstatSync(join(dest, 'link-to-dir')).isSymbolicLink());
assert.strictEqual(readFileSync(join(dest, 'link-to-dir', 'nested.txt'), 'utf8'), 'nested');

// A dangling link has no target to copy.
const dangling = nextdir();
mkdirSync(dangling, { recursive: true });
symlinkSync(join(target, 'missing.txt'), join(dangling, 'link'));
assert.throws(
  () => cpSync(dangling, nextdir(),
               mustNotMutateObjectDeep({ dereference: true, recursive: true })),
  { code: 'ENOENT' },
);

// A symlink cycle fails with ELOOP instead of recursing indefinitely.
const looping = nextdir();
mkdirSync(looping, { recursive: true });
symlinkSync(looping, join(looping, 'loop'), 'dir');
assert.throws(
  () => cpSync(looping, nextdir(),
               mustNotMutateObjectDeep({ dereference: true, recursive: true })),
  { code: 'ELOOP' },
);

// Under force, an existing destination link is replaced rather than written
// through. Whether replacement happens at all still follows force and
// errorOnExist.
function withDestLink() {
  const outside = nextdir();
  const from = nextdir();
  const to = nextdir();
  mkdirSync(outside, { recursive: true });
  mkdirSync(from, { recursive: true });
  mkdirSync(to, { recursive: true });
  writeFileSync(join(outside, 'untouched.txt'), 'untouched', 'utf8');
  symlinkSync(join(target, 'file.txt'), join(from, 'entry'));
  symlinkSync(join(outside, 'untouched.txt'), join(to, 'entry'));
  return { outside, from, to };
}

{
  const { outside, from, to } = withDestLink();
  cpSync(from, to, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
  assert(!lstatSync(join(to, 'entry')).isSymbolicLink());
  assert.strictEqual(readFileSync(join(to, 'entry'), 'utf8'), 'file');
  assert.strictEqual(readFileSync(join(outside, 'untouched.txt'), 'utf8'), 'untouched');
}

{
  const { outside, from, to } = withDestLink();
  cpSync(from, to, mustNotMutateObjectDeep({
    dereference: true, recursive: true, force: false,
  }));
  assert(lstatSync(join(to, 'entry')).isSymbolicLink());
  assert.strictEqual(readFileSync(join(outside, 'untouched.txt'), 'utf8'), 'untouched');
}

{
  const { outside, from, to } = withDestLink();
  assert.throws(
    () => cpSync(from, to, mustNotMutateObjectDeep({
      dereference: true, recursive: true, force: false, errorOnExist: true,
    })),
    { code: 'ERR_FS_CP_EEXIST' },
  );
  assert(lstatSync(join(to, 'entry')).isSymbolicLink());
  assert.strictEqual(readFileSync(join(outside, 'untouched.txt'), 'utf8'), 'untouched');
}

// A link resolving to a directory descends into whatever already occupies the
// destination path: a file there fails the way copying into it fails, and a
// link to a directory is followed and merged into.
{
  const from = nextdir();
  const to = nextdir();
  mkdirSync(from, { recursive: true });
  mkdirSync(to, { recursive: true });
  symlinkSync(join(target, 'dir'), join(from, 'entry'), 'dir');
  writeFileSync(join(to, 'entry'), 'occupied', 'utf8');
  assert.throws(
    () => cpSync(from, to,
                 mustNotMutateObjectDeep({ dereference: true, recursive: true })),
    { code: 'ENOTDIR' },
  );
}

{
  const existing = nextdir();
  const from = nextdir();
  const to = nextdir();
  mkdirSync(existing, { recursive: true });
  mkdirSync(from, { recursive: true });
  mkdirSync(to, { recursive: true });
  writeFileSync(join(existing, 'kept.txt'), 'kept', 'utf8');
  symlinkSync(join(target, 'dir'), join(from, 'entry'), 'dir');
  symlinkSync(existing, join(to, 'entry'), 'dir');

  cpSync(from, to, mustNotMutateObjectDeep({ dereference: true, recursive: true }));

  assert(lstatSync(join(to, 'entry')).isSymbolicLink());
  assert.strictEqual(readFileSync(join(existing, 'kept.txt'), 'utf8'), 'kept');
  assert.strictEqual(readFileSync(join(existing, 'nested.txt'), 'utf8'), 'nested');
}

// A directory occupying the destination path is an occupied destination like
// any other, so the same force and errorOnExist rules decide its fate.
function withDestDir() {
  const from = nextdir();
  const to = nextdir();
  mkdirSync(from, { recursive: true });
  mkdirSync(join(to, 'entry'), { recursive: true });
  symlinkSync(join(target, 'file.txt'), join(from, 'entry'));
  return { from, to };
}

{
  const { from, to } = withDestDir();
  cpSync(from, to, mustNotMutateObjectDeep({ dereference: true, recursive: true }));
  assert(lstatSync(join(to, 'entry')).isFile());
  assert.strictEqual(readFileSync(join(to, 'entry'), 'utf8'), 'file');
}

{
  const { from, to } = withDestDir();
  cpSync(from, to, mustNotMutateObjectDeep({
    dereference: true, recursive: true, force: false,
  }));
  assert(lstatSync(join(to, 'entry')).isDirectory());
}

{
  const { from, to } = withDestDir();
  assert.throws(
    () => cpSync(from, to, mustNotMutateObjectDeep({
      dereference: true, recursive: true, force: false, errorOnExist: true,
    })),
    { code: 'ERR_FS_CP_EEXIST' },
  );
  assert(lstatSync(join(to, 'entry')).isDirectory());
}
