// This tests that cpSync allows file to be copied to a file path.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, lstatSync } from 'node:fs';
import { join } from 'node:path';
import fixtures from '../common/fixtures.js';

const srcFile = fixtures.path('copy/kitchen-sink/index.js');
const destFile = join(nextdir(), 'index.js');
cpSync(srcFile, destFile, mustNotMutateObjectDeep({ dereference: true }));
const stat = lstatSync(destFile);
assert(stat.isFile());
