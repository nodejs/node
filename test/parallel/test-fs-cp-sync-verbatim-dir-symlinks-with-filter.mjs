// This tests that cpSync with verbatimSymlinks and filter preserves
// the directory symlink type on Windows (does not create a file symlink).
import { mustNotMutateObjectDeep, isWindows } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import {
  cpSync,
  mkdirSync,
  writeFileSync,
  symlinkSync,
  readlinkSync,
  readdirSync,
  statSync,
} from 'node:fs';
import { join } from 'node:path';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

// Setup source with a relative directory symlink
const src = nextdir();
mkdirSync(join(src, 'packages', 'my-lib'), mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'packages', 'my-lib', 'index.js'), 'module.exports = "hello"');
mkdirSync(join(src, 'linked'), mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(join('..', 'packages', 'my-lib'), join(src, 'linked', 'my-lib'), 'dir');

// Copy with verbatimSymlinks: true AND a filter function
const dest = nextdir();
cpSync(src, dest, mustNotMutateObjectDeep({
  recursive: true,
  verbatimSymlinks: true,
  filter: () => true,
}));

// Verify the symlink target is preserved verbatim
const link = readlinkSync(join(dest, 'linked', 'my-lib'));
if (isWindows) {
  assert.strictEqual(link.toLowerCase(), join('..', 'packages', 'my-lib').toLowerCase());
} else {
  assert.strictEqual(link, join('..', 'packages', 'my-lib'));
}

// Verify the symlink works as a directory (not a file symlink)
const destSymlink = join(dest, 'linked', 'my-lib');
assert.ok(statSync(destSymlink).isDirectory(),
  'symlink target should be accessible as a directory');
assert.deepStrictEqual(readdirSync(destSymlink), ['index.js']);
