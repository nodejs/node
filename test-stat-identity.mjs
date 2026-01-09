// test-stat-identity.mjs
import { statSync, rmSync } from 'fs';
import { mkdirSync, symlinkSync } from 'fs';
import { join, resolve } from 'path';

const tmp = resolve('tmp_stat_test');
try {
  rmSync(tmp, { recursive: true, force: true });
} catch {}
try {
  mkdirSync(tmp);
} catch {}

const dir = join(tmp, 'real_dir');
try {
  mkdirSync(dir);
} catch {}

const link1 = join(tmp, 'link1');
// Link points to 'real_dir' which is in the same folder as 'link1'
try {
  symlinkSync('real_dir', link1);
} catch {}

try {
  const s1 = statSync(dir);
  const s2 = statSync(link1);

  console.log(`Real: dev=${s1.dev}, ino=${s1.ino}`);
  console.log(`Link: dev=${s2.dev}, ino=${s2.ino}`);
  console.log(`Match? ${s1.dev === s2.dev && s1.ino === s2.ino}`);
} catch (e) {
  console.error(e);
}
