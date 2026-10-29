// This tests that cpSync gives the directories it creates the mode of the
// corresponding source directory, as cp does.
import { mustNotMutateObjectDeep, isWindows, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { chmodSync, cpSync, mkdirSync, statSync, writeFileSync, promises } from 'node:fs';
import { join } from 'node:path';
import { isMainThread } from 'node:worker_threads';
import tmpdir from '../common/tmpdir.js';

if (isWindows)
  skip('directory modes are not meaningful on Windows');
if (!isMainThread)
  skip('process.umask() is not available in workers');

tmpdir.refresh();
const mask = process.umask(0o022);

const src = nextdir();
mkdirSync(join(src, 'private', 'inner'), { recursive: true, mode: 0o700 });
mkdirSync(join(src, 'shared'), { mode: 0o775 });
writeFileSync(join(src, 'private', 'inner', 'file'), 'x', { mode: 0o600 });

function modes(root) {
  return ['.', 'private', 'private/inner', 'shared', 'private/inner/file']
    .map((p) => (statSync(join(root, p)).mode & 0o777).toString(8));
}

const destSync = nextdir();
cpSync(src, destSync, mustNotMutateObjectDeep({ recursive: true }));
assert.deepStrictEqual(modes(destSync), modes(src));

const destAsync = nextdir();
await promises.cp(src, destAsync, { recursive: true });
assert.deepStrictEqual(modes(destAsync), modes(src));

// A read-only source directory can still be copied; its copy ends up read-only too.
{
  const roSrc = nextdir();
  mkdirSync(join(roSrc, 'sub'), { recursive: true });
  writeFileSync(join(roSrc, 'sub', 'file'), 'x');
  chmodSync(join(roSrc, 'sub'), 0o555);
  chmodSync(roSrc, 0o555);
  const readOnly = [roSrc, join(roSrc, 'sub')];
  for (const copy of [(dest) => cpSync(roSrc, dest, { recursive: true }),
                      (dest) => promises.cp(roSrc, dest, { recursive: true })]) {
    const dest = nextdir();
    await copy(dest);
    assert.strictEqual(statSync(join(dest, 'sub', 'file')).size, 1);
    assert.deepStrictEqual(
      [dest, join(dest, 'sub')].map((p) => (statSync(p).mode & 0o777).toString(8)), ['555', '555']);
    readOnly.push(dest, join(dest, 'sub'));
  }
  // Let tmpdir clean up.
  for (const dir of readOnly) chmodSync(dir, 0o755);
}

// An existing destination directory keeps its own mode.
const existing = nextdir();
mkdirSync(existing, { mode: 0o711 });
cpSync(src, existing, mustNotMutateObjectDeep({ recursive: true }));
assert.strictEqual((statSync(existing).mode & 0o777).toString(8), '711');
assert.deepStrictEqual(modes(existing).slice(1), modes(src).slice(1));
process.umask(mask);
