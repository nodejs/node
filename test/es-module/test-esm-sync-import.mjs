// Flags: --no-warnings
import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';


describe('synchronous ESM loading', () => {
  it('should create minimal promises for ESM importing ESM', async () => {
    // import-esm.mjs imports imported-esm.mjs — a pure ESM graph.
    const count = await getPromiseCount(fixtures.path('es-modules', 'import-esm.mjs'));
    // V8's Module::Evaluate returns one promise for the entire graph.
    assert.strictEqual(count, 1);
  });

  it('should create minimal promises for ESM importing CJS', async () => {
    // builtin-imports-case.mjs imports node:assert (builtin) + dep1.js and dep2.js (CJS).
    const count = await getPromiseCount(fixtures.path('es-modules', 'builtin-imports-case.mjs'));
    // V8 creates one promise for the ESM entry evaluation, plus one per CJS module
    // in the graph (each CJS namespace is wrapped in a promise).
    // entry (ESM, 1) + node:assert (CJS, 1) + dep1.js (CJS, 1) + dep2.js (CJS, 1) = 4.
    assert.strictEqual(count, 4);
  });

  it('should fall back to async evaluation for top-level await', async () => {
    // tla/resolved.mjs uses top-level await, so the sync path detects TLA
    // and falls back to async evaluation.
    const count = await getPromiseCount(fixtures.path('es-modules', 'tla', 'resolved.mjs'));
    // The async fallback creates more promises — just verify the module
    // still runs successfully. The promise count will be higher than the
    // sync path but should remain bounded.
    assert(count > 1, `Expected TLA fallback to create multiple promises, got ${count}`);
  });

  it('should create minimal promises when entry point is CJS importing ESM', async () => {
    // When a CJS entry point uses require(esm), the ESM module is loaded via
    // ModuleJobSync, so the same promise minimization applies.
    const count = await getPromiseCount(fixtures.path('es-modules', 'require-esm-entry.cjs'));
    // V8's Module::Evaluate returns one promise for the ESM module.
    assert.strictEqual(count, 1);
  });
});


async function getPromiseCount(entry) {
  const { stdout, stderr, code } = await spawnPromisified(process.execPath, [
    '--require', fixtures.path('es-modules', 'promise-counter.cjs'),
    entry,
  ]);
  assert.strictEqual(code, 0, `child failed:\nstdout: ${stdout}\nstderr: ${stderr}`);
  const match = stdout.match(/PROMISE_COUNT=(\d+)/);
  assert(match, `Expected PROMISE_COUNT in output, got: ${stdout}`);
  return Number(match[1]);
}
