// Regression test for https://github.com/nodejs/node/issues/61878
// fs.cpSync should copy files when destination path has UTF characters.
import '../common/index.mjs';
import { cpSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import assert from 'node:assert';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

const src = join(tmpdir.path, 'src');
mkdirSync(join(src, 'subdir'), { recursive: true });
writeFileSync(join(src, 'file1.txt'), 'Hello World');
writeFileSync(join(src, 'subdir', 'nested.txt'), 'Nested File');

const dest = join(tmpdir.path, 'Eyjafjallajökull-Pranckevičius');
cpSync(src, dest, { recursive: true, force: true });

const destFiles = readdirSync(dest);
assert.ok(destFiles.includes('file1.txt'));
assert.strictEqual(readFileSync(join(dest, 'file1.txt'), 'utf8'), 'Hello World');
assert.ok(destFiles.includes('subdir'));
assert.strictEqual(readFileSync(join(dest, 'subdir', 'nested.txt'), 'utf8'), 'Nested File');
