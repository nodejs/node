// An `exclude` function receives a Dirent when `withFileTypes` is true. That
// Dirent must describe the path inside `options.cwd`, not a path with the same
// name inside `process.cwd()`.
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { glob, globSync, mkdirSync, writeFileSync } from 'node:fs';
import { glob as globPromise } from 'node:fs/promises';
import { promisify } from 'node:util';

tmpdir.refresh();

// `a` is a directory in `cwd` and a file in the process working directory.
const cwd = tmpdir.resolve('root');
const processCwd = tmpdir.resolve('ambient');
mkdirSync(tmpdir.resolve('root', 'a'), { recursive: true });
mkdirSync(processCwd);
writeFileSync(tmpdir.resolve('root', 'a', 'real.txt'), '');
writeFileSync(tmpdir.resolve('ambient', 'a'), '');
process.chdir(processCwd);

function options() {
  const seen = [];
  return {
    seen,
    options: {
      cwd,
      withFileTypes: true,
      exclude(dirent) {
        seen.push(dirent);
        return dirent.isDirectory();
      },
    },
  };
}

function check(seen, matches) {
  assert.deepStrictEqual(matches, []);
  assert.strictEqual(seen.length, 1);
  assert.strictEqual(seen[0].name, 'a');
  assert.strictEqual(seen[0].parentPath, cwd);
  assert.strictEqual(seen[0].isDirectory(), true);
}

{
  const { seen, options: opts } = options();
  check(seen, globSync('a/**', opts));
}

{
  const { seen, options: opts } = options();
  check(seen, await promisify(glob)('a/**', opts));
}

{
  const { seen, options: opts } = options();
  check(seen, await Array.fromAsync(globPromise('a/**', opts)));
}
