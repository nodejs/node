// This tests that cpSync accepts file URL as src and dest.
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir, assertDirEquivalent } from '../common/fs.js';
import { cpSync } from 'node:fs';
import { pathToFileURL } from 'node:url';
import tmpdir from '../common/tmpdir.js';
import fixtures from '../common/fixtures.js';

tmpdir.refresh();

const src = fixtures.path('copy/kitchen-sink');
const dest = nextdir();
cpSync(pathToFileURL(src), pathToFileURL(dest), mustNotMutateObjectDeep({ recursive: true }));
assertDirEquivalent(src, dest);
