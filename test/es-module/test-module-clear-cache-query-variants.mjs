import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache } from 'node:module';

const url = new URL('../fixtures/module-cache/esm-query-counter.mjs', import.meta.url);

const first = await import(`${url.href}?v=1`);
const second = await import(`${url.href}?v=2`);
assert.strictEqual(first.count, 1);
assert.strictEqual(second.count, 2);

const result = clearCache(url);
assert.strictEqual(result.commonjs, false);
assert.strictEqual(result.module, true);

const third = await import(`${url.href}?v=1`);
assert.strictEqual(third.count, 3);

delete globalThis.__module_cache_esm_query_counter;
