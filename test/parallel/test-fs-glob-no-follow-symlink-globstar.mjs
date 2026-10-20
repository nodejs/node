import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { glob as globPromise, mkdir, symlink, writeFile } from 'node:fs/promises';
import { globSync, glob as asyncGlob } from 'node:fs';

if (common.isWindows) {
  // Directory symlinks require elevated privileges on Windows.
  common.skip('symlinks are unreliable on Windows');
}

tmpdir.refresh();

// Layout:
//   root/visible.txt
//   root/link  ->  outside   (outside/ holds files that must NOT be listed)
const base = tmpdir.resolve('globstar-symlink');
const root = path.resolve(base, 'workspace');
const outside = path.resolve(base, 'outside');
const outsideNested = path.resolve(outside, 'nested');

await mkdir(root, { recursive: true });
await mkdir(outsideNested, { recursive: true });
await writeFile(path.resolve(root, 'visible.txt'), 'visible');
await writeFile(path.resolve(outside, 'secret-one.txt'), 'secret one');
await writeFile(path.resolve(outsideNested, 'secret-two.txt'), 'secret two');
await symlink(outside, path.resolve(root, 'link'), 'dir');

const expected = ['link', 'visible.txt'].map((e) => e.replaceAll('/', path.sep)).sort();

// `**/*` must not descend into the symlinked directory, neither by default nor
// with followSymlinks explicitly disabled.
assert.deepStrictEqual(globSync('**/*', { cwd: root }).sort(), expected);
assert.deepStrictEqual(
  globSync('**/*', { cwd: root, followSymlinks: false }).sort(),
  expected,
);

const promiseMatches = [];
for await (const entry of globPromise('**/*', { cwd: root, followSymlinks: false })) {
  promiseMatches.push(entry);
}
assert.deepStrictEqual(promiseMatches.sort(), expected);

const callbackMatches = await new Promise((resolve, reject) => {
  asyncGlob('**/*', { cwd: root, followSymlinks: false }, (err, matches) => {
    if (err) reject(err);
    else resolve(matches);
  });
});
assert.deepStrictEqual(callbackMatches.sort(), expected);

// Under the permission model, `**/*` must not expose entries from a symlink
// target that is outside the granted read set.
if (common.hasCrypto) {
  const child = `
    const assert = require('node:assert');
    const fs = require('node:fs');
    const path = require('node:path');
    const root = ${JSON.stringify(root)};
    const outside = ${JSON.stringify(outside)};
    const expected = ['link', 'visible.txt'].map((e) => e.replaceAll('/', path.sep)).sort();
    assert.strictEqual(process.permission.has('fs.read', root), true);
    assert.strictEqual(process.permission.has('fs.read', outside), false);
    assert.throws(() => fs.readdirSync(outside), { code: 'ERR_ACCESS_DENIED' });
    assert.deepStrictEqual(fs.globSync('**/*', { cwd: root }).sort(), expected);
    assert.deepStrictEqual(
      fs.globSync('**/*', { cwd: root, followSymlinks: false }).sort(),
      expected,
    );
  `;
  const { status, stdout, stderr } = spawnSync(process.execPath, [
    '--permission',
    `--allow-fs-read=${root}`,
    '-e',
    child,
  ], { encoding: 'utf8' });
  assert.strictEqual(status, 0, stderr || stdout);
}
