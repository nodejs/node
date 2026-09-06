// Tests race conditions between clearCache and concurrent dynamic imports
// of a CJS module loaded via import().
//
// Scenarios covered:
//   A) clearCache fires BEFORE the in-flight import promise settles:
//        p = import(url); clearCache(url); result = await p
//      The original import must still succeed with the first module instance.
//
//   B) Two concurrent imports (sharing the same in-flight job) with clearCache
//      between them, then a third import after clearing:
//        p1 = import(url)          → job1 created, cached
//        p2 = import(url)          → reuses job1 (same in-flight promise)
//        clearCache(url)           → removes job1 from cache
//        p3 = import(url)          → job3 created (fresh execution)
//        [await all three]
//      p1 and p2 must resolve to the SAME module instance (they shared job1).
//      p3 must resolve to a DIFFERENT, freshly-executed module instance.
//
//   C) clearCache fires AFTER the import has fully settled and another clear +
//      import is done serially — basic sanity check that repeated cycles work.

import '../common/index.mjs';
import assert from 'node:assert';
import { clearCache } from 'node:module';

const url = new URL('../fixtures/module-cache/cjs-counter.js', import.meta.url);

// ── Scenario A: clearCache before in-flight import settles ──────────────────

const p_a = import(url.href);  // in-flight; module not yet resolved
clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
});

const result_a = await p_a;
// Scenario A: in-flight import must still resolve to the first instance.
assert.strictEqual(result_a.default.count, 1);

// ── Scenario B: two concurrent imports share a job; clearCache between ──────

// Re-seed for a clean counter baseline.
clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
});

delete globalThis.__module_cache_cjs_counter;

// Both p_b1 and p_b2 start before clearCache → they share the same in-flight job.
const p_b1 = import(url.href);
const p_b2 = import(url.href);

clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
});

// p_b3 starts after clearCache → gets a fresh independent job.
const p_b3 = import(url.href);

const [r_b1, r_b2, r_b3] = await Promise.all([p_b1, p_b2, p_b3]);

// p_b1 and p_b2 shared the same in-flight job → identical module namespace.
assert.strictEqual(r_b1, r_b2);
// Scenario B: shared job resolves to the first (re-seeded) instance.
assert.strictEqual(r_b1.default.count, 1);

// p_b3 was created after clearCache → fresh execution, different instance.
assert.notStrictEqual(r_b3, r_b1);
assert.strictEqual(r_b3.default.count, 2);

// ── Scenario C: serial repeated cycles ─────────────────────────────────────

clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
});
const r_c1 = await import(url.href);
assert.strictEqual(r_c1.default.count, 3);

clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
});
const r_c2 = await import(url.href);
assert.strictEqual(r_c2.default.count, 4);

delete globalThis.__module_cache_cjs_counter;
