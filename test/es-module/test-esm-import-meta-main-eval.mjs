import { spawnPromisified } from '../common/index.mjs';
import assert from "node:assert/strict";

const execOptions = ['--input-type', 'module'];

const script = `
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true);

const { isMain: importedModuleIsMain } = await import('../fixtures/es-modules/import-meta-main.mjs');
assert.strictEqual(importedModuleIsMain, false);
`;

const scriptInWorker = `
import { Worker } from 'node:worker_threads';
new Worker(\`${script}\`, { eval: true });
`;

async function test_evaluated_script() {
  const { stderr, code } = await spawnPromisified(process.execPath, [...execOptions, '--eval', script], { cwd: import.meta.dirname })
  assert.strictEqual(stderr, "")
  assert.strictEqual(code, 0)
}

async function test_evaluated_worker_script() {
  const { stderr, code } = await spawnPromisified(process.execPath, [...execOptions, '--eval', scriptInWorker], { cwd: import.meta.dirname })
  assert.strictEqual(stderr, "")
  assert.strictEqual(code, 0)
}

await test_evaluated_script();
await test_evaluated_worker_script();
