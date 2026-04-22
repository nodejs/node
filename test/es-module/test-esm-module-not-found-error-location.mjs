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

const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [entry]);

assert.strictEqual(code, 1);
assert.strictEqual(signal, null);
assert.strictEqual(stdout, '');
assert.ok(stderr.includes(`${entry}:2`));
assert.match(stderr, /import missing from "this-package-does-not-exist";/);
assert.match(stderr, / {21}\^{27}/);
assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
