import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.js';
import assert from 'node:assert/strict';

const importMetaMainScript = `
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true, 'import.meta.main should evaluate true in main module');

const { isMain: importedModuleIsMain } = await import(
    ${JSON.stringify(fixtures.fileURL('es-modules/import-meta-main.mjs'))}
);
assert.strictEqual(importedModuleIsMain, false, 'import.meta.main should evaluate false in imported module');
`;

function wrapScriptInEvalWorker(script) {
  return `
  import { Worker } from 'node:worker_threads';
  new Worker(${JSON.stringify(script)}, { eval: true });
  `;
}

function convertScriptSourceToDataUrl(script) {
  return new URL(`data:text/javascript,${encodeURIComponent(script)}`);
}

function wrapScriptInUrlWorker(script) {
  return `
  import { Worker } from 'node:worker_threads';
  new Worker(new URL("${convertScriptSourceToDataUrl(script)}"));
  `;
}

async function test_evaluated_script() {
  const result = await spawnPromisified(
    process.execPath,
    ['--input-type=module', '--eval', importMetaMainScript],
  );
  assert.deepStrictEqual(result, {
    stderr: '',
    stdout: '',
    code: 0,
    signal: null,
  });
}

async function test_evaluated_script_in_eval_worker() {
  const result = await spawnPromisified(
    process.execPath,
    ['--input-type=module', '--eval', wrapScriptInEvalWorker(importMetaMainScript)],
  );
  assert.deepStrictEqual(result, {
    stderr: '',
    stdout: '',
    code: 0,
    signal: null,
  });
}

async function test_evaluated_script_in_url_worker() {
  const result = await spawnPromisified(
    process.execPath,
    ['--input-type=module', '--eval', wrapScriptInUrlWorker(importMetaMainScript)],
  );
  assert.deepStrictEqual(result, {
    stderr: '',
    stdout: '',
    code: 0,
    signal: null,
  });
}

await test_evaluated_script();
await test_evaluated_script_in_eval_worker();
await test_evaluated_script_in_url_worker();
