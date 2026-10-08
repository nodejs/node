// Dropping a permission while an async walk is under way is honored by
// the rest of the walk, which checks paths off the main thread. The walk
// runs in a child process because the drop is process-wide.
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { mkdir, writeFile } from 'node:fs/promises';
import { join } from 'node:path';

tmpdir.refresh();
// The directory visited first (last in listing order) holds more than one
// batch of results; the others are only reached after the drop.
const cwd = tmpdir.resolve('tree');
for (const dir of ['a', 'b', 'zz']) {
  await mkdir(join(cwd, dir), { recursive: true });
  await writeFile(join(cwd, dir, 'file.txt'), '');
}
for (let i = 0; i < 300; i++) {
  await writeFile(join(cwd, 'zz', `${i}.txt`), '');
}

const script = `
  const assert = require('node:assert');
  const dc = require('node:diagnostics_channel');
  const { glob } = require('node:fs/promises');
  const { join, sep } = require('node:path');
  const cwd = process.argv[1];
  const denied = [];
  dc.subscribe('node:permission-model:fs', (message) => {
    assert.strictEqual(message.permission, 'FileSystemRead');
    denied.push(message.resource);
  });
  (async () => {
    const results = [];
    for await (const entry of glob('**/*.txt', { cwd })) {
      if (results.length === 0) {
        assert.ok(process.permission.has('fs.read', cwd));
        process.permission.drop('fs.read');
        assert.ok(!process.permission.has('fs.read', cwd));
      }
      results.push(entry);
    }
    assert.strictEqual(results.length, 301);
    assert.ok(results.every((entry) => entry.startsWith('zz' + sep)));
    assert.ok(denied.includes(join(cwd, 'a')), 'a was denied and reported');
    assert.ok(denied.includes(join(cwd, 'b')), 'b was denied and reported');
    console.log('ok');
  })();
`;
const child = spawnSync(process.execPath, [
  '--permission', '--allow-fs-read=*', '-e', script, cwd,
], { encoding: 'utf8' });
assert.strictEqual(child.status, 0, child.stderr);
assert.strictEqual(child.stdout.trim(), 'ok');
