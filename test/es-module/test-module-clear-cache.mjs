import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache } from 'node:module';

const specifier = '../fixtures/module-cache/esm-counter.mjs';

const first = await import(specifier);
const second = await import(specifier);

assert.strictEqual(first.count, 1);
assert.strictEqual(second.count, 1);
assert.strictEqual(first, second);

const result = clearCache(specifier, { parentURL: import.meta.url });
assert.strictEqual(result.cjs, false);
assert.strictEqual(result.esm, true);

const third = await import(specifier);
assert.strictEqual(third.count, 2);

delete globalThis.__module_cache_esm_counter;
