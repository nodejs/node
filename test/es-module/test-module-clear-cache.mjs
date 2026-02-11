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
assert.strictEqual(result.commonjs, false);
assert.strictEqual(result.module, true);

const third = await import(specifier);
assert.strictEqual(third.count, 2);

const nested = new URL('../fixtures/module-cache/esm-nested-a.mjs', import.meta.url);
const nestedFirst = await import(`${nested.href}?v=1`);
assert.strictEqual(nestedFirst.value, 1);

const nestedResult = clearCache(new URL('../fixtures/module-cache/esm-nested-b.mjs', import.meta.url));
assert.strictEqual(nestedResult.commonjs, false);
assert.strictEqual(nestedResult.module, true);

const nestedSecond = await import(`${nested.href}?v=2`);
assert.strictEqual(nestedSecond.value, 2);
assert.strictEqual(globalThis.__module_cache_esm_nested_c_counter, 1);

delete globalThis.__module_cache_esm_counter;
delete globalThis.__module_cache_esm_nested_b_counter;
delete globalThis.__module_cache_esm_nested_c_counter;
