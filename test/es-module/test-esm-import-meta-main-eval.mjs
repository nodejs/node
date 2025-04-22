import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.js';
import assert from 'node:assert/strict';

const execOptions = ['--input-type', 'module'];

const script = `
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true);

const { isMain: importedModuleIsMain } = await import(
    ${JSON.stringify(fixtures.fileURL('es-modules/import-meta-main.mjs'))}
);
assert.strictEqual(importedModuleIsMain, false);
`;

const scriptInWorker = `
import { Worker } from 'node:worker_threads';
new Worker(${JSON.stringify(script)}, { eval: true });
`;

async function test_evaluated_script() {
  const result = await spawnPromisified(
    process.execPath,
    [...execOptions, '--eval', script],
  );
  assert.deepStrictEqual(result, {
    stderr: '',
    stdout: '',
    code: 0,
    signal: null,
  });
}

async function test_evaluated_worker_script() {
  const result = await spawnPromisified(
    process.execPath,
    [...execOptions, '--eval', scriptInWorker],
  );
  assert.deepStrictEqual(result, {
    stderr: '',
    stdout: '',
    code: 0,
    signal: null,
  });
}

await test_evaluated_script();
await test_evaluated_worker_script();
