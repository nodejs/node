// Flags: --expose-internals
// Verifies the V8-level memory-retention behaviour of clearCache when a module
// is statically imported by a still-live parent.
//
// BACKGROUND:
//   When a parent module P statically imports M (via `import … from './M'`),
//   V8's module instantiation creates an internal strong reference from P's
//   compiled module record to M's module record.  This link is permanent for
//   the lifetime of P.  Clearing M from Node.js's JS-level caches (loadCache /
//   resolveCache) does NOT sever the V8-internal link; the old ModuleWrap for
//   M stays alive as long as P is alive.
//
//   Consequence:  after `clearCache(M)` + `await import(M)`, TWO module
//   instances coexist:
//     - M_old : retained by P's V8-internal link, never re-executed by P
//     - M_new : created by the fresh import(), seen by all NEW importers
//
//   This is a *bounded* leak (one stale instance per cleared module per live
//   static parent), not an unbounded one.  It is unavoidable given the
//   ECMA-262 HostLoadImportedModule idempotency requirement.
//
//   For purely dynamic imports (no static parent holding them) the old
//   ModuleWrap IS collectible after clearCache — see the second half of this
//   test which uses checkIfCollectableByCounting to confirm that.

import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache, createRequire } from 'node:module';
import { queryObjects } from 'v8';

// Use createRequire to access CJS-only internals from this ESM file.
const require = createRequire(import.meta.url);
const { checkIfCollectableByCounting } = require('../common/gc');
const { internalBinding } = require('internal/test/binding');
const { ModuleWrap } = internalBinding('module_wrap');

const counterURL = new URL(
  '../fixtures/module-cache/esm-counter.mjs',
  import.meta.url,
).href;

const parentURL = new URL(
  '../fixtures/module-cache/esm-static-parent.mjs',
  import.meta.url,
).href;

// ── Part 1 : static-parent split-brain ──────────────────────────────────────
// Load the static parent, which in turn statically imports esm-counter.mjs.

const parent = await import(parentURL);
assert.strictEqual(parent.count, 1);  // counter runs once

// Snapshot the number of live ModuleWraps before clearing.
const wrapsBefore = queryObjects(ModuleWrap, { format: 'count' });

// Clear the counter's Node-side caches (does NOT sever V8 static links).
clearCache(counterURL, {
  parentURL: import.meta.url,
  resolver: 'import',
});

// Re-import counter: a fresh instance is created and executed.
const fresh = await import(counterURL);
assert.strictEqual(fresh.count, 2);   // New execution.
// Parent still sees the OLD instance via the V8-internal static link —
// the "split-brain" behaviour.
assert.strictEqual(parent.count, 1);

// After the fresh import there should be MORE live ModuleWraps than before,
// because the old instance (held by the parent) was NOT collected.
const wrapsAfter = queryObjects(ModuleWrap, { format: 'count' });
assert.ok(
  wrapsAfter > wrapsBefore,
  `Expected more live ModuleWraps after re-import (old instance retained ` +
  `by static parent).  before=${wrapsBefore}, after=${wrapsAfter}`,
);

// ── Part 2 : dynamic-only modules ARE collectible ───────────────────────────
// Prove that for purely-dynamic imports (no static parent), cleared modules
// can be garbage-collected.  This confirms that the static-parent case is the
// source of the memory retention, not clearCache itself.

const baseURL = new URL(
  '../fixtures/module-cache/esm-counter.mjs',
  import.meta.url,
).href;

const outer = 8;
const inner = 4;

await checkIfCollectableByCounting(async (i) => {
  for (let j = 0; j < inner; j++) {
    const url = `${baseURL}?leak-test=${i}-${j}`;
    await import(url);
    clearCache(url, {
      parentURL: import.meta.url,
      resolver: 'import',
    });
  }
  return inner;
}, ModuleWrap, outer, 50);

delete globalThis.__module_cache_esm_counter;
