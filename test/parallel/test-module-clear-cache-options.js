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
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Missing parentURL throws.
assert.throws(() => clearCache(fixture, {
  resolver: 'require',
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// Missing resolver throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Invalid resolver throws.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'invalid',
}), {
  code: 'ERR_INVALID_ARG_VALUE',
});

// Valid call with relative specifier and file URL parentURL.
clearCache(relativeSpecifier, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});
assert.strictEqual(require.cache[fixture], undefined);

const first = require(fixture);
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
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
}), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// importAttributes must be an object if provided.
assert.throws(() => clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
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
});
assert.strictEqual(require.cache[unloaded], undefined);

// Clearing the same module twice — second call is a no-op.
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});

// URL object specifier with resolver: 'require' is a no-op.
// CJS does not interpret file: URLs as paths; hooks would be needed to handle them.
const third = require(fixture);
assert.strictEqual(third.count, 4);
clearCache(pathToFileURL(fixture), {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});
// Cache should still be populated — URL specifiers are not resolved for CJS.
assert.notStrictEqual(require.cache[fixture], undefined);

// String file: URL specifier with resolver: 'require' is also a no-op.
clearCache(pathToFileURL(fixture).href, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});
// Cache should still be populated.
assert.notStrictEqual(require.cache[fixture], undefined);

// parentURL as string URL (not URL object) — still works when specifier is a path.
clearCache(fixture, {
  parentURL: pathToFileURL(__filename).href,
  resolver: 'require',
});
assert.strictEqual(require.cache[fixture], undefined);

// Re-require to bump count for subsequent assertions.
const fourth = require(fixture);
assert.strictEqual(fourth.count, 5);

delete globalThis.__module_cache_cjs_counter;
