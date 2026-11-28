// On Windows, an absolute glob pattern without a drive letter, such as
// '\foo\*' or '/foo/*', is rooted at the current drive, like
// path.resolve('\\foo'). It must find the same entries as the pattern with
// the drive letter, and results must use '\' throughout.

import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { glob, globSync, mkdirSync, writeFileSync } from 'node:fs';
import { glob as asyncGlob } from 'node:fs/promises';
import { join } from 'node:path';
import { test } from 'node:test';
import { promisify } from 'node:util';

if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
}

tmpdir.refresh();

if (!/^[A-Za-z]:\\/.test(tmpdir.path)) {
  common.skip('tmpdir is not on a drive letter');
}

// Make the fixture's drive the current one.
process.chdir(tmpdir.path);

const dir = tmpdir.resolve('no-drive');
mkdirSync(dir);
writeFileSync(join(dir, 'a.js'), '');
writeFileSync(join(dir, 'b.js'), '');

// 'C:\...\no-drive' -> '\...\no-drive'
const noDrive = dir.slice(2);
const expected = [join(noDrive, 'a.js'), join(noDrive, 'b.js')];

const apis = [
  ['glob', promisify(glob)],
  ['globSync', globSync],
  ['fsPromises glob', (pattern) => Array.fromAsync(asyncGlob(pattern))],
];

for (const [name, runGlob] of apis) {
  test(`${name} - backslashes`, async () => {
    const actual = await runGlob(`${noDrive}\\*.js`);
    assert.deepStrictEqual(actual.sort(), expected);
  });

  test(`${name} - forward slashes`, async () => {
    const actual = await runGlob(`${noDrive.replaceAll('\\', '/')}/*.js`);
    assert.deepStrictEqual(actual.sort(), expected);
  });

  // A literal path is looked up directly instead of through a directory
  // listing.
  test(`${name} - literal path`, async () => {
    const actual = await runGlob(join(noDrive, 'a.js'));
    assert.deepStrictEqual(actual, [join(noDrive, 'a.js')]);
  });
}
