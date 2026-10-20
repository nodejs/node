// Flags: --no-warnings
import '../common/index.mjs';
import { spawnSyncAndAssert } from '../common/child_process.js';
import * as fixtures from '../common/fixtures.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';

// Promise counts are implementation details tracked for regressions, not API guarantees.

describe('synchronous ESM loading', () => {
  it('should create minimal promises for ESM importing ESM', async () => {
    const count = getPromiseCount(fixtures.path('es-modules', 'import-esm.mjs'));
    assert.strictEqual(count, 1);
  });

  it('should create minimal promises for ESM importing CJS', async () => {
    const count = getPromiseCount(fixtures.path('es-modules', 'builtin-imports-case.mjs'));
    assert.strictEqual(count, 4);
  });

  it('should fall back to async evaluation for top-level await', async () => {
    const count = getPromiseCount(fixtures.path('es-modules', 'tla', 'resolved.mjs'));
    assert(count > 1, `Expected TLA fallback to create multiple promises, got ${count}`);
  });

  it('should create minimal promises when entry point is CJS importing ESM', async () => {
    const count = getPromiseCount(fixtures.path('es-modules', 'require-esm-entry.cjs'));
    assert.strictEqual(count, 1);
  });
});

function getPromiseCount(entry) {
  const { stderr } = spawnSyncAndAssert(process.execPath, [
    '--trace-promises',
    entry,
  ], {
    stderr: /created promise #/,
  });
  return stderr.match(/created promise #/g)?.length ?? 0;
}
