import '../common/index.mjs';
import { strictEqual } from 'node:assert';
import fs from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import { createRequire } from 'node:module';
const tmpdir = createRequire(import.meta.url)('../common/tmpdir.js');

tmpdir.refresh();

const code = 'process.stdout.write(this === undefined ? "module" : "commonjs");\n';

// Copying would be significantly slower
const copyMethod = async (src, dest) => fs.link(src, dest);

const pathJS = tmpdir.resolve('blep.js');
await fs.writeFile(pathJS, code);
const pathMJS = tmpdir.resolve('blep.mjs');
await fs.writeFile(pathMJS, code);
const pathCJS = tmpdir.resolve('blep.cjs');
await fs.writeFile(pathCJS, code);
const path = tmpdir.resolve('blep');
await fs.writeFile(path, code);
const pathIDK = tmpdir.resolve('blep.idontknowthistype');
await fs.writeFile(pathIDK, code);

{
  const execPath = tmpdir.resolve('node');
  await copyMethod(process.execPath, execPath);

  strictEqual(execFileSync(execPath, [pathJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [pathMJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathCJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [path]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [pathIDK]).toString(), 'commonjs');
}

{
  const execPath = tmpdir.resolve('node-esm');
  await copyMethod(process.execPath, execPath);

  strictEqual(execFileSync(execPath, [pathJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathMJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathCJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [path]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathIDK]).toString(), 'module');
}

{
  const execPath = tmpdir.resolve('node.exe');
  await copyMethod(process.execPath, execPath);

  strictEqual(execFileSync(execPath, [pathJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [pathMJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathCJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [path]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [pathIDK]).toString(), 'commonjs');
}

{
  const execPath = tmpdir.resolve('node-esm.exe');
  await copyMethod(process.execPath, execPath);

  strictEqual(execFileSync(execPath, [pathJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathMJS]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathCJS]).toString(), 'commonjs');
  strictEqual(execFileSync(execPath, [path]).toString(), 'module');
  strictEqual(execFileSync(execPath, [pathIDK]).toString(), 'module');
}
