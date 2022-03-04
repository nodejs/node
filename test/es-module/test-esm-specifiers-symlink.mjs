import * as common from '../common/index.mjs';
import path from 'path';
import fs from 'fs/promises';
import tmpdir from '../common/tmpdir.js';
import { spawn } from 'child_process';
import assert from 'assert';

tmpdir.refresh();
const tmpDir = tmpdir.path;

// Create the following file structure:
// ├── index.mjs
// ├── subfolder
// │   ├── index.mjs
// │   └── node_modules
// │       └── package-a
// │           └── index.mjs
// └── symlink.mjs -> ./subfolder/index.mjs
const entry = path.join(tmpDir, 'index.mjs');
const symlink = path.join(tmpDir, 'symlink.mjs');
const real = path.join(tmpDir, 'subfolder', 'index.mjs');
const packageDir = path.join(tmpDir, 'subfolder', 'node_modules', 'package-a');
const packageEntry = path.join(packageDir, 'index.mjs');
try {
  await fs.symlink(real, symlink);
} catch (err) {
  if (err.code !== 'EPERM') throw err;
  common.skip('insufficient privileges for symlinks');
}
await fs.mkdir(packageDir, { recursive: true });
await Promise.all([
  fs.writeFile(entry, 'import "./symlink.mjs";'),
  fs.writeFile(real, 'export { a } from "package-a/index.mjs"'),
  fs.writeFile(packageEntry, 'export const a = 1;'),
]);

spawn(process.execPath, ['--experimental-specifier-resolution=node', entry],
      { stdio: 'inherit' }).on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 0);
}));
