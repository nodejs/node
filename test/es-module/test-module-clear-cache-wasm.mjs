// Flags: --no-warnings

import '../common/index.mjs';
import assert from 'node:assert';
import { clearCache } from 'node:module';

const url = new URL('../fixtures/simple.wasm', import.meta.url);

const first = await import(url.href);
assert.strictEqual(first.add(1, 2), 3);

clearCache(url, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'module',
});

const second = await import(url.href);
assert.strictEqual(second.add(2, 3), 5);
assert.notStrictEqual(first, second);
