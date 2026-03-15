import '../common/index.mjs';
import assert from 'node:assert';
import { clearCache } from 'node:module';

const url = new URL('../fixtures/module-cache/cjs-counter.js', import.meta.url);

const importPromise = import(url.href);
clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'module',
});

const first = await importPromise;
assert.strictEqual(first.default.count, 1);

clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'module',
});
const second = await import(url.href);
assert.strictEqual(second.default.count, 2);

delete globalThis.__module_cache_cjs_counter;
