// This tests that cp() gives copied files the mode of their source, including
// the setuid, setgid and sticky bits, as cpSync() does.
import { isWindows, skip } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { chmodSync, cpSync, mkdirSync, statSync, writeFileSync, promises } from 'node:fs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

if (isWindows)
  skip('file modes are not meaningful on Windows');

tmpdir.refresh();
const modes = { suid: 0o4755, sgid: 0o2755, sticky: 0o1755, private: 0o600 };
const src = nextdir();
mkdirSync(src);
for (const [name, mode] of Object.entries(modes)) {
  writeFileSync(join(src, name), 'x');
  chmodSync(join(src, name), mode);
}
const modesIn = (dir) => Object.fromEntries(
  Object.keys(modes).map((name) => [name, statSync(join(dir, name)).mode & 0o7777]));
const expected = modesIn(src);

const viaCp = nextdir();
await promises.cp(src, viaCp, { recursive: true });
assert.deepStrictEqual(modesIn(viaCp), expected);

const viaCpTimestamps = nextdir();
await promises.cp(src, viaCpTimestamps, { recursive: true, preserveTimestamps: true });
assert.deepStrictEqual(modesIn(viaCpTimestamps), expected);

const viaCpSync = nextdir();
cpSync(src, viaCpSync, { recursive: true });
assert.deepStrictEqual(modesIn(viaCpSync), expected);
