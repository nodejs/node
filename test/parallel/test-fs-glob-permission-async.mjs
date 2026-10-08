// Flags: --permission --allow-fs-read=* --allow-fs-write=*

// Under the permission model every path is checked on the main thread,
// so the async glob APIs walk in short main-thread slices between which
// the event loop turns, instead of finishing the walk before returning.
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { glob, globSync } from 'node:fs';
import { glob as globIterator, mkdir, writeFile } from 'node:fs/promises';
import { dirname, join, sep } from 'node:path';
import { promisify } from 'node:util';

assert.ok(process.permission.has('fs.read'));

tmpdir.refresh();
const cwd = tmpdir.resolve('tree');
const files = ['a/b/c/d.txt', 'a/b/e.txt', 'a/f/g.txt', 'h/i.txt', 'j.txt'];
for (const file of files) {
  await mkdir(join(cwd, dirname(file)), { recursive: true });
  await writeFile(join(cwd, file), '');
}
const expected = files.map((file) => file.replaceAll('/', sep)).sort();
assert.deepStrictEqual(globSync('**/*.txt', { cwd }).sort(), expected);

{
  // The walk starts after the call returns.
  let completed = false;
  const pending = new Promise((resolve) => {
    glob('**/*.txt', { cwd }, common.mustSucceed((results) => {
      completed = true;
      assert.deepStrictEqual(results.sort(), expected);
      resolve();
    }));
  });
  assert.strictEqual(completed, false);
  await pending;
  assert.deepStrictEqual((await promisify(glob)('**/*.txt', { cwd })).sort(), expected);
  const results = [];
  for await (const entry of globIterator('**/*.txt', { cwd })) results.push(entry);
  assert.deepStrictEqual(results.sort(), expected);
}

{
  // With an exclude callback that overruns a slice's time budget, the
  // loop turns between directories.
  function stall(ms) {
    const end = performance.now() + ms;
    while (performance.now() < end);
  }
  let turn = 0;
  let walking = true;
  (function tick() {
    turn++;
    if (walking) setImmediate(tick);
  })();
  const turns = new Set();
  const exclude = (name) => {
    turns.add(turn);
    stall(2);
    return false;
  };
  assert.deepStrictEqual((await promisify(glob)('**/*.txt', { cwd, exclude })).sort(), expected);
  walking = false;
  assert.ok(turns.size >= 2, `exclude ran within ${turns.size} loop turn(s)`);
}
