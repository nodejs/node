// An `exclude` callback can only run on the main thread. The async APIs
// still must not hold the event loop for the whole walk: the walk
// advances in short main-thread slices between which the loop turns.
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { glob, globSync } from 'node:fs';
import { glob as globIterator, mkdir, writeFile } from 'node:fs/promises';
import { dirname, join, sep } from 'node:path';
import { promisify } from 'node:util';

tmpdir.refresh();
const cwd = tmpdir.resolve('tree');
const files = ['a/b/c/d.txt', 'a/b/e.txt', 'a/f/g.txt', 'h/i.txt', 'j.txt'];
for (const file of files) {
  await mkdir(join(cwd, dirname(file)), { recursive: true });
  await writeFile(join(cwd, file), '');
}
const expected = files.map((file) => file.replaceAll('/', sep)).sort();
assert.deepStrictEqual(globSync('**/*.txt', { cwd }).sort(), expected);

// Long enough that the slice it runs in overruns its time budget, so the
// next directory is visited from a later loop turn.
function stall(ms) {
  const end = performance.now() + ms;
  while (performance.now() < end);
}

// Counts loop turns for as long as the walks run.
let turn = 0;
let walking = true;
(function tick() {
  turn++;
  if (walking) setImmediate(tick);
})();

// Records the loop turns its calls landed in.
function makeExclude() {
  const turns = new Set();
  const exclude = common.mustCallAtLeast((name) => {
    assert.strictEqual(typeof name, 'string');
    turns.add(turn);
    stall(2);
    return name === 'f';
  }, 1);
  return { exclude, turns };
}
const expectedExcluded = expected.filter((file) => !file.includes(`${sep}f${sep}`));

{
  // Callback API
  const { exclude, turns } = makeExclude();
  await new Promise((resolve) => {
    glob('**/*.txt', { cwd, exclude }, common.mustSucceed((results) => {
      assert.deepStrictEqual(results.sort(), expectedExcluded);
      resolve();
    }));
    // Nothing of the walk runs before the call returns.
    assert.strictEqual(turns.size, 0);
  });
  assert.ok(turns.size >= 2, `exclude ran within ${turns.size} loop turn(s)`);
}

{
  // Promisified callback API
  const { exclude, turns } = makeExclude();
  const pending = promisify(glob)('**/*.txt', { cwd, exclude });
  assert.strictEqual(turns.size, 0);
  assert.deepStrictEqual((await pending).sort(), expectedExcluded);
  assert.ok(turns.size >= 2, `exclude ran within ${turns.size} loop turn(s)`);
}

{
  // Async iterator
  const { exclude, turns } = makeExclude();
  const results = [];
  for await (const entry of globIterator('**/*.txt', { cwd, exclude })) {
    results.push(entry);
  }
  assert.deepStrictEqual(results.sort(), expectedExcluded);
  assert.ok(turns.size >= 2, `exclude ran within ${turns.size} loop turn(s)`);
}

{
  // Leaving the iterator early cancels a walk that is not over: the
  // directory visited first (the last in listing order) holds more
  // entries than one batch, so it fills the first batch while the other
  // directories are still queued.
  const many = tmpdir.resolve('many');
  for (const dir of ['a', 'b', 'zz']) {
    await mkdir(join(many, dir), { recursive: true });
    await writeFile(join(many, dir, 'file.txt'), '');
  }
  for (let i = 0; i < 300; i++) {
    await writeFile(join(many, 'zz', `${i}.txt`), '');
  }
  let calls = 0;
  const exclude = () => {
    calls++;
    return false;
  };
  let seen = 0;
  for await (const entry of globIterator('**/*.txt', { cwd: many, exclude })) {
    assert.ok(entry.endsWith('.txt'), entry);
    if (++seen === 2) break;
  }
  assert.strictEqual(seen, 2);
  const callsAtBreak = calls;
  await new Promise((resolve) => setTimeout(resolve, 20));
  assert.strictEqual(calls, callsAtBreak);
}

{
  // A callback that throws fails the walk with its error.
  const error = new Error('exclude failed');
  const exclude = common.mustCall(() => { throw error; });
  glob('**/*.txt', { cwd, exclude }, common.mustCall((err, results) => {
    assert.strictEqual(err, error);
    assert.strictEqual(results, undefined);
  }));
  await assert.rejects(
    promisify(glob)('**/*.txt', { cwd, exclude: common.mustCall(() => { throw error; }) }),
    (err) => err === error);
  await assert.rejects(
    (async () => {
      for await (const entry of globIterator('**/*.txt', { cwd, exclude: common.mustCall(() => { throw error; }) })) {
        assert.fail(`unexpected result ${entry}`);
      }
    })(),
    (err) => err === error);
}

{
  // withFileTypes: the callback sees Dirents.
  const { exclude: base, turns } = makeExclude();
  const exclude = (dirent) => base(dirent.name);
  const results = await promisify(glob)('**/*.txt', { cwd, exclude, withFileTypes: true });
  assert.deepStrictEqual(
    results.map((dirent) => join(dirent.parentPath, dirent.name).slice(cwd.length + 1)).sort(),
    expectedExcluded);
  assert.ok(turns.size >= 2, `exclude ran within ${turns.size} loop turn(s)`);
}

walking = false;
