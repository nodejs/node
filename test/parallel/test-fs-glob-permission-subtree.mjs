// With read access granted to one subtree, a glob rooted above it finds
// nothing (the root listing is denied) and a glob rooted inside it works;
// the async walk checks paths off the main thread and reports denials
// through the diagnostics channel from the main thread.
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';

tmpdir.refresh();
const cwd = tmpdir.resolve('tree');
const files = ['a/b/c.txt', 'a/d.txt', 'e.txt'];
for (const file of files) {
  await mkdir(join(cwd, dirname(file)), { recursive: true });
  await writeFile(join(cwd, file), '');
}

const script = `
  const assert = require('node:assert');
  const dc = require('node:diagnostics_channel');
  const { glob, globSync } = require('node:fs');
  const { promisify } = require('node:util');
  const { join } = require('node:path');
  const cwd = process.argv[1];
  const denied = [];
  dc.subscribe('node:permission-model:fs', (message) => denied.push(message.resource));
  (async () => {
    assert.deepStrictEqual(globSync('**/*.txt', { cwd }), []);
    assert.deepStrictEqual(await promisify(glob)('**/*.txt', { cwd }), []);
    assert.deepStrictEqual(denied.filter((path) => path === cwd).length, 2);
    denied.length = 0;
    const inside = await promisify(glob)('**/*.txt', { cwd: join(cwd, 'a') });
    assert.deepStrictEqual(inside.sort(), [join('b', 'c.txt'), 'd.txt']);
    assert.deepStrictEqual(denied, []);
    console.log('ok');
  })();
`;
const child = spawnSync(process.execPath, [
  '--permission', `--allow-fs-read=${join(cwd, 'a')}`, '-e', script, cwd,
], { encoding: 'utf8' });
assert.strictEqual(child.status, 0, child.stderr);
assert.strictEqual(child.stdout.trim(), 'ok');
