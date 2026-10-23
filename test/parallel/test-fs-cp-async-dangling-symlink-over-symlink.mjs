import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { promises, readlinkSync, symlinkSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = nextdir();
const dest = nextdir();
symlinkSync('missing', src);
symlinkSync(process.execPath, dest);

await promises.cp(src, dest, mustNotMutateObjectDeep({ force: true }));
assert.strictEqual(readlinkSync(dest), resolve(dirname(src), 'missing'));
