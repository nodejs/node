import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import fs from 'node:fs/promises';
import { describe, test, before } from 'node:test';
import { Worker } from 'node:worker_threads';

const accessInternalsSource = `
import 'node:internal/freelist';
`;

function convertScriptSourceToDataUrl(script) {
  return new URL(`data:text/javascript,${encodeURIComponent(script)}`);
}

describe('Worker threads should not be able to access internal modules', () => {
  before(() => tmpdir.refresh());

  test('worker instantiated with module file path', async () => {
    const moduleFilepath = tmpdir.resolve('test-worker-internal-modules.mjs');
    await fs.writeFile(moduleFilepath, accessInternalsSource);
    const w = new Worker(moduleFilepath);
    await assert.rejects(once(w, 'exit'), { code: 'ERR_UNKNOWN_BUILTIN_MODULE' });
  });

  test('worker instantiated with module source', async () => {
    const w = new Worker(accessInternalsSource, { eval: true });
    await assert.rejects(once(w, 'exit'), { code: 'ERR_UNKNOWN_BUILTIN_MODULE' });
  });

  test('worker instantiated with data: URL', async () => {
    const w = new Worker(convertScriptSourceToDataUrl(accessInternalsSource));
    await assert.rejects(once(w, 'exit'), { code: 'ERR_UNKNOWN_BUILTIN_MODULE' });
  });
});
