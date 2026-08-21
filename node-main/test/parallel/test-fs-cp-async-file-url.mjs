// This tests that it accepts file URL as src and dest.

import { mustCall, mustNotMutateObjectDeep } from '../common/index.mjs';
import assert from 'node:assert';
import { cp } from 'node:fs';
import { pathToFileURL } from 'node:url';
import tmpdir from '../common/tmpdir.js';
import { assertDirEquivalent, nextdir } from '../common/fs.js';

tmpdir.refresh();

const src = './test/fixtures/copy/kitchen-sink';
const dest = nextdir();
cp(pathToFileURL(src), pathToFileURL(dest), mustNotMutateObjectDeep({ recursive: true }),
   mustCall((err) => {
     assert.strictEqual(err, null);
     assertDirEquivalent(src, dest);
   }));
