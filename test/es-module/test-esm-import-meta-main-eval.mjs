import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.js';
import assert from 'node:assert/strict';
import { describe, it } from 'node:test';

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
  new Worker(new URL(${JSON.stringify(convertScriptSourceToDataUrl(script))}));
  `;
}

describe('import.meta.main in evaluated scripts', () => {
  const importMetaMainScript = `
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true, 'import.meta.main should evaluate true in main module');

const { isMain: importedModuleIsMain } = await import(
    ${JSON.stringify(fixtures.fileURL('es-modules/import-meta-main.mjs'))}
);
assert.strictEqual(importedModuleIsMain, false, 'import.meta.main should evaluate false in imported module');
`;

  it('should evaluate true in evaluated script', async () => {
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
  });

  it('should evaluate true in worker instantiated with module source by evaluated script', async () => {
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
  });

  it('should evaluate true in worker instantiated with `data:` URL by evaluated script', async () => {
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
  });
});

describe('import.meta.main in typescript scripts', () => {
  const importMetaMainTSScript = `
import assert from 'node:assert/strict';

assert.strictEqual(import.meta.main, true, 'import.meta.main should evaluate true in main module');

const { isMain: importedModuleIsMain } = await import(
    ${JSON.stringify(fixtures.fileURL('es-modules/import-meta-main.ts'))}
);
assert.strictEqual(importedModuleIsMain, false, 'import.meta.main should evaluate false in imported module');
`;

  it('should evaluate true in evaluated script', async () => {
    const result = await spawnPromisified(
      process.execPath,
      ['--experimental-strip-types',
       '--input-type=module-typescript',
       '--disable-warning=ExperimentalWarning',
       '--eval', importMetaMainTSScript],
    );
    assert.deepStrictEqual(result, {
      stderr: '',
      stdout: '',
      code: 0,
      signal: null,
    });
  });

  it('should evaluate true in worker instantiated with module source by evaluated script', async () => {
    const result = await spawnPromisified(
      process.execPath,
      ['--experimental-strip-types',
       '--input-type=module-typescript',
       '--disable-warning=ExperimentalWarning',
       '--eval', wrapScriptInEvalWorker(importMetaMainTSScript)],
    );
    assert.deepStrictEqual(result, {
      stderr: '',
      stdout: '',
      code: 0,
      signal: null,
    });
  });

  it('should evaluate true in worker instantiated with `data:` URL by evaluated script', async () => {
    const result = await spawnPromisified(
      process.execPath,
      ['--input-type=module',
       '--experimental-strip-types',
       '--input-type=module-typescript',
       '--disable-warning=ExperimentalWarning',
       '--eval', wrapScriptInUrlWorker(importMetaMainTSScript)],
    );
    assert.deepStrictEqual(result, {
      stderr: '',
      stdout: '',
      code: 0,
      signal: null,
    });
  });
});
