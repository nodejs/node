import { spawnPromisified } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import { writeFile } from 'node:fs/promises';
import { execPath } from 'node:process';

tmpdir.refresh();

const entry = tmpdir.resolve('entry.mjs');
await writeFile(entry, [
  'import ok from "node:fs";',
  'import missing from "this-package-does-not-exist";',
  'console.log(ok, missing);',
].join('\n'));

async function assertModuleNotFoundLocation(args) {
  const { code, signal, stdout, stderr } = await spawnPromisified(execPath, args);

  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdout, '');
  assert.ok(stderr.includes(`${entry}:2`));
  assert.match(stderr, /import missing from "this-package-does-not-exist";/);
  assert.match(stderr, / {21}\^{27}/);
  assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
}

await assertModuleNotFoundLocation([entry]);

await assertModuleNotFoundLocation([
  '--no-warnings',
  '--import',
  'data:text/javascript,' +
    'import{register}from"node:module";' +
    'register("data:text/javascript,' +
    'export async function resolve(s,c,n){return n(s,c)}")',
  entry,
]);
