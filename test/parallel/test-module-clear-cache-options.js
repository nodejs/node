'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');
const relativeSpecifier = '../fixtures/module-cache/cjs-counter.js';

require(fixture);

// parentURL must be a valid URL, not a file path.
assert.throws(() => clearCache(relativeSpecifier, {
  parentURL: __filename,
  resolver: 'require',
  caches: 'module',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Missing parentURL throws.
assert.throws(() => clearCache(fixture, {
  resolver: 'require',
  caches: 'module',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Missing resolver throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  caches: 'module',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Missing caches throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Invalid resolver throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'invalid',
  caches: 'module',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Invalid caches throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'invalid',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Valid call with relative specifier and file URL parentURL.
clearCache(relativeSpecifier, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
assert.strictEqual(require.cache[fixture], undefined);

const first = require(fixture);
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
const second = require(fixture);
assert.notStrictEqual(first.count, second.count);
assert.strictEqual(first.count, 2);
assert.strictEqual(second.count, 3);

// --- Additional validation edge cases ---

// Missing options entirely.
assert.throws(() => clearCache(fixture), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Options must be an object.
assert.throws(() => clearCache(fixture, 'bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Specifier must be a string or URL.
assert.throws(() => clearCache(123, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// importAttributes must be an object if provided.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
  importAttributes: 'bad',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// --- No-op scenarios (should not throw) ---

// Clearing a module that was never loaded — no-op.
const unloaded = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-child.js');
assert.strictEqual(require.cache[unloaded], undefined);
clearCache(unloaded, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
assert.strictEqual(require.cache[unloaded], undefined);

// Clearing the same module twice — second call is a no-op.
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});

// URL object specifier with resolver: 'require'.
const third = require(fixture);
assert.strictEqual(third.count, 4);
clearCache(pathToFileURL(fixture), {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
assert.strictEqual(require.cache[fixture], undefined);

// String URL specifier with resolver: 'require'.
const fourth = require(fixture);
assert.strictEqual(fourth.count, 5);
clearCache(pathToFileURL(fixture).href, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});
assert.strictEqual(require.cache[fixture], undefined);

// parentURL as string URL (not URL object).
const fifth = require(fixture);
assert.strictEqual(fifth.count, 6);
clearCache(fixture, {
  parentURL: pathToFileURL(__filename).href,
  resolver: 'require',
  caches: 'module',
});
assert.strictEqual(require.cache[fixture], undefined);

delete globalThis.__module_cache_cjs_counter;
