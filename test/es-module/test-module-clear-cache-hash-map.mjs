// Flags: --expose-internals
// Evaluates internal book-keeping memory behaviour across clearCache cycles.
//
// Node.js keeps internal book-keeping for compiled ES modules so that a
// compiled module can be looked up again while it is still alive. Entries are
// added when a module is compiled and removed when that module is
// garbage-collected. Clearing the Node-side load cache does not directly
// touch this book-keeping — entries go away only when the corresponding
// module objects are collected.
//
// We verify two invariants:
//
//  1. DYNAMIC imports: after clearCache + GC, the old compiled module is
//     collected and therefore its internal book-keeping entry is removed.
//     Book-keeping does NOT grow without bound for purely-dynamic
//     import/clear cycles.
//     (Verified via checkIfCollectableByCounting.)
//
//  2. STATIC imports: when a parent P statically imports M, clearing M from
//     the load cache does not free M's compiled module (the static link
//     keeps it). Each re-import adds one new entry while the old entry stays
//     for the lifetime of P. This is a bounded, expected retention (not an
//     unbounded leak): it is capped at one stale entry per module per live
//     static parent.

import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache, createRequire } from 'node:module';
import { queryObjects } from 'v8';

const require = createRequire(import.meta.url);
const { checkIfCollectableByCounting } = require('../common/gc');
const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

const counterBase = new URL(
  '../fixtures/module-cache/esm-counter.mjs',
  import.meta.url,
).href;

const parentURL = new URL(
  '../fixtures/module-cache/esm-static-parent.mjs',
  import.meta.url,
).href;

// ── Invariant 1: dynamic-only cycles do NOT leak compiled modules ───────────
// Use cache-busting query params so each import gets a distinct URL.

const outer = 8;
const inner = 4;

await checkIfCollectableByCounting(async (i) => {
  for (let j = 0; j < inner; j++) {
    const url = `${counterBase}?hm=${i}-${j}`;
    await import(url);
    clearCache(url, {
      parentURL: import.meta.url,
      resolver: 'import',
    });
  }
  return inner;
}, ModuleWrap, outer, 50);

// ── Invariant 2: static-parent cycles cause bounded retention ───────────────
// After loading the static parent (which pins one counter instance), each
// clear+re-import of the base counter URL creates exactly one new compiled
// module while the old one stays alive (pinned by the parent).
// The net growth per cycle is +1. After N cycles the live count is
//   baseline + 1(parent) + 1(pinned original counter) + 1(current counter)
// — a constant overhead, not growing with N.

// Load the static parent; this also loads the counter (count starts at 1 for
// the global, but we seed it fresh by clearing any earlier runs' state).
delete globalThis.__module_cache_esm_counter;

const parent = await import(parentURL);
assert.strictEqual(parent.count, 1);

const wrapCount0 = queryObjects(ModuleWrap, { format: 'count' });

// Cycle 1: clear counter + re-import → new instance created, old pinned.
clearCache(counterBase, {
  parentURL: import.meta.url,
  resolver: 'import',
});
const v2 = await import(counterBase);
assert.strictEqual(v2.count, 2);

const wrapCount1 = queryObjects(ModuleWrap, { format: 'count' });
// +1 new compiled module (v2); old one kept alive by parent's static link.
assert.strictEqual(wrapCount1, wrapCount0 + 1,
                   'Each clear+reimport cycle adds exactly one new compiled ' +
                   'module when a static parent holds the old instance');

// Cycle 2: clear counter again + re-import.
clearCache(counterBase, {
  parentURL: import.meta.url,
  resolver: 'import',
});
const v3 = await import(counterBase);
assert.strictEqual(v3.count, 3);

const wrapCount2 = queryObjects(ModuleWrap, { format: 'count' });
// Another +1 (v3); v2 is no longer in the load cache and has no other strong
// holder, so it MAY have been collected already. v1 (pinned by parent) is
// still alive. Net growth is bounded by the number of active versions in
// any live strong reference — typically just the current one plus the
// parent-pinned original.
assert.ok(
  wrapCount2 <= wrapCount1 + 1,
  `After a second cycle, live compiled-module count should grow by at most 1 ` +
  `(got ${wrapCount2}, was ${wrapCount1})`,
);

delete globalThis.__module_cache_esm_counter;
