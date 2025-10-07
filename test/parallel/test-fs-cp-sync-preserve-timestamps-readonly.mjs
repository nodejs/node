// This tests that cpSync makes file writeable when updating timestamp, if not writeable.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, writeFileSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import { setTimeout } from 'node:timers/promises';

import tmpdir from '../common/tmpdir.js';
tmpdir.refresh();

const src = nextdir();
mkdirSync(src, mustNotMutateObjectDeep({ recursive: true }));
const dest = nextdir();
mkdirSync(dest, mustNotMutateObjectDeep({ recursive: true }));
writeFileSync(join(src, 'foo.txt'), 'foo', mustNotMutateObjectDeep({ mode: 0o444 }));
// Small wait to make sure that destStat.mtime.getTime() would produce a time
// different from srcStat.mtime.getTime() if preserveTimestamps wasn't set to true
await setTimeout(5);
cpSync(src, dest, mustNotMutateObjectDeep({ preserveTimestamps: true, recursive: true }));
assertDirEquivalent(src, dest);
const srcStat = lstatSync(join(src, 'foo.txt'));
const destStat = lstatSync(join(dest, 'foo.txt'));
assert.strictEqual(srcStat.mtime.getTime(), destStat.mtime.getTime());
