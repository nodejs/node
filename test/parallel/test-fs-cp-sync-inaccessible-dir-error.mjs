// This tests that cpSync converts native directory enumeration failures into
// JavaScript exceptions.
import * as common from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { execFileSync, spawnSync } from 'node:child_process';
import { cpSync, existsSync, mkdirSync, rmSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import tmpdir from '../common/tmpdir.js';

if (!common.isWindows) {
  common.skip('Windows ACL specific test');
}

function run(command, args) {
  return execFileSync(command, args, {
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
  });
}

function currentWindowsUser() {
  return run('whoami', []).trim();
}

function restrictDirectory(dir) {
  const user = currentWindowsUser();
  run('icacls', [dir, '/deny', `${user}:(OI)(CI)(RX)`]);
}

function restoreDirectory(dir) {
  if (!existsSync(dir)) return;

  const user = currentWindowsUser();
  run('icacls', [dir, '/remove:d', user]);
}

if (process.argv[2] === 'child') {
  const src = process.argv[3];
  const dest = process.argv[4];

  assert.throws(
    () => cpSync(src, dest, { recursive: true }),
    { name: 'Error' }
  );
  process.exit(0);
}

tmpdir.refresh();

const root = nextdir();
const src = join(root, 'src');
const dest = join(root, 'dest');
const restrictedDir = join(src, 'restricted');
mkdirSync(restrictedDir, { recursive: true });
writeFileSync(join(src, 'readable.txt'), 'readable\n');
writeFileSync(join(restrictedDir, 'blocked.txt'), 'blocked\n');

restrictDirectory(restrictedDir);

try {
  const filename = fileURLToPath(import.meta.url);
  const child = spawnSync(process.execPath,
                          [filename, 'child', src, dest],
                          { encoding: 'utf8' });
  assert.strictEqual(
    child.status,
    0,
    `child exited with ${child.status}\nstdout:\n${child.stdout}\nstderr:\n${child.stderr}`
  );
} finally {
  restoreDirectory(restrictedDir);
  rmSync(root, { recursive: true, force: true });
}
