// A glob whose first step is a literal directory (`somedir/*`) must still
// match when read access is granted only to that directory. Listing the
// parent is denied, but the walk can probe the granted child directly.
// `**` from the parent still matches nothing: that pattern has to list.
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { mkdir, writeFile } from 'node:fs/promises';
import { join } from 'node:path';

tmpdir.refresh();
const cwd = tmpdir.resolve('tree');
const somedir = join(cwd, 'somedir');
await mkdir(somedir, { recursive: true });
await writeFile(join(somedir, 'file1.js'), '');

const script = `
  const assert = require('node:assert');
  const { glob, globSync } = require('node:fs');
  const { glob: globIterator } = require('node:fs/promises');
  const { join } = require('node:path');
  const { promisify } = require('node:util');
  const expected = [join('somedir', 'file1.js')];
  (async () => {
    assert.deepStrictEqual(globSync('*'), []);
    assert.deepStrictEqual(globSync('**/*'), []);
    assert.deepStrictEqual(globSync('somedir/*'), expected);
    assert.deepStrictEqual(await promisify(glob)('somedir/*'), expected);
    const entries = [];
    for await (const entry of globIterator('somedir/*')) entries.push(entry);
    assert.deepStrictEqual(entries, expected);
    console.log('ok');
  })();
`;
const child = spawnSync(process.execPath, [
  '--permission', `--allow-fs-read=${somedir}`, '-e', script,
], { cwd, encoding: 'utf8' });
assert.strictEqual(child.status, 0, child.stderr);
assert.strictEqual(child.stdout.trim(), 'ok');
