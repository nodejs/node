import { spawnPromisified } from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import { writeFile } from 'node:fs/promises';
import { execPath } from 'node:process';

tmpdir.refresh();

const missingPackage = 'this-package-does-not-exist';

async function assertDynamicImportNotFoundLocation(entry, sourceLine, underline) {
  const { code, signal, stdout, stderr } = await spawnPromisified(execPath, [entry]);

  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdout, '');
  assert.ok(stderr.includes(`${entry}:2`));
  assert.match(stderr, new RegExp(sourceLine));
  assert.match(stderr, underline);
  assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
}

const esmEntry = tmpdir.resolve('dynamic-entry.mjs');
await writeFile(esmEntry, [
  'const ok = 1;',
  `await import(${JSON.stringify(missingPackage)});`,
  'console.log(ok);',
].join('\n'));

await assertDynamicImportNotFoundLocation(
  esmEntry,
  `await import\\(${JSON.stringify(missingPackage)}\\);`,
  / {14}\^{27}/,
);

const cjsEntry = tmpdir.resolve('dynamic-entry.cjs');
await writeFile(cjsEntry, [
  'const ok = 1;',
  `import(${JSON.stringify(missingPackage)});`,
  'ok;',
].join('\n'));

await assertDynamicImportNotFoundLocation(
  cjsEntry,
  `import\\(${JSON.stringify(missingPackage)}\\);`,
  / {8}\^{27}/,
);
