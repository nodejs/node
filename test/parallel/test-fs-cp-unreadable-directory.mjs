// This tests that cp() and cpSync() report an unreadable directory inside the
// source tree as an error instead of terminating the process.
import { isWindows, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { chmodSync, cpSync, mkdirSync, readdirSync, writeFileSync, promises } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

if (isWindows)
  skip('no way to make a directory unreadable');
if (process.getuid() === 0)
  skip('root can read the directory anyway');

tmpdir.refresh();
const src = nextdir();
mkdirSync(join(src, 'locked'), { recursive: true });
writeFileSync(join(src, 'file'), 'x');
chmodSync(join(src, 'locked'), 0o000);
try {
  readdirSync(join(src, 'locked'));
  chmodSync(join(src, 'locked'), 0o700);
  skip('the directory is still readable');
} catch {
  // Expected: it is unreadable.
}

try {
  assert.throws(() => cpSync(src, nextdir(), { recursive: true }), { code: 'EACCES' });
  await assert.rejects(promises.cp(src, nextdir(), { recursive: true }), { code: 'EACCES' });
} finally {
  chmodSync(join(src, 'locked'), 0o700);
}
