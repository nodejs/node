// This tests that copying the same directory twice succeeds when it contains a
// symlink to a directory.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import {
  cp,
  cpSync,
  lstatSync,
  mkdirSync,
  realpathSync,
  symlinkSync,
} from 'node:fs';
import { cp as cpPromise } from 'node:fs/promises';
import { join } from 'node:path';
import { promisify } from 'node:util';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const target = nextdir();
const src = nextdir();
mkdirSync(target, mustNotMutateObjectDeep({ recursive: true }));
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
symlinkSync(target, join(src, 'link'));

const options = () => mustNotMutateObjectDeep({ recursive: true });
const filterOptions = () => mustNotMutateObjectDeep({
  filter: () => true,
  recursive: true,
});
const copyCallback = promisify(cp);
const destinations = [nextdir(), nextdir(), nextdir(), nextdir()];

cpSync(src, destinations[0], options());
cpSync(src, destinations[0], options());

cpSync(src, destinations[1], filterOptions());
cpSync(src, destinations[1], filterOptions());

await copyCallback(src, destinations[2], options());
await copyCallback(src, destinations[2], options());

await cpPromise(src, destinations[3], options());
await cpPromise(src, destinations[3], options());

for (const dest of destinations) {
  const link = join(dest, 'link');
  assert(lstatSync(link).isSymbolicLink());
  assert.strictEqual(realpathSync(link), realpathSync(target));
}
