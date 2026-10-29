// Flags: --permission-audit

// In audit mode every denied check is published to the diagnostics
// channel and the access goes ahead. The async glob walks off the main
// thread and publishes the denials it met from the main thread, so a
// subscriber sees them and the walk still finds everything.
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import { glob, globSync } from 'node:fs';
import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, join, sep } from 'node:path';
import { promisify } from 'node:util';

tmpdir.refresh();
const cwd = tmpdir.resolve('tree');
const files = ['a/b/c.txt', 'a/d.txt', 'e.txt'];
for (const file of files) {
  await mkdir(join(cwd, dirname(file)), { recursive: true });
  await writeFile(join(cwd, file), '');
}
const expected = files.map((file) => file.replaceAll('/', sep)).sort();

const denied = [];
dc.subscribe('node:permission-model:fs', common.mustCallAtLeast((message) => {
  assert.strictEqual(message.permission, 'FileSystemRead');
  denied.push(message.resource);
}, 2));

assert.deepStrictEqual(globSync('**/*.txt', { cwd }).sort(), expected);
assert.ok(denied.includes(cwd), `sync walk reported ${cwd}`);
assert.ok(denied.includes(join(cwd, 'a')), `sync walk reported ${join(cwd, 'a')}`);

denied.length = 0;
assert.deepStrictEqual((await promisify(glob)('**/*.txt', { cwd })).sort(), expected);
assert.ok(denied.includes(cwd), `async walk reported ${cwd}`);
assert.ok(denied.includes(join(cwd, 'a')), `async walk reported ${join(cwd, 'a')}`);
assert.ok(denied.includes(join(cwd, 'a', 'b')), `async walk reported ${join(cwd, 'a', 'b')}`);
