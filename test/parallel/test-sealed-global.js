'use strict';

// This tests that the globals can still be loaded after globalThis is freezed.

require('../common');
const assert = require('assert');

Object.seal(globalThis);
const keys = Reflect.ownKeys(globalThis).filter((k) => typeof k === 'string');

// These failures come from undici. We can remove them once they are fixed.
const knownFailures = new Set(['FormData', 'Headers', 'Request', 'Response']);
const failures = new Map();
const success = new Map();
for (const key of keys) {
  try {
    success.set(key, globalThis[key]);
  } catch (e) {
    failures.set(key, e);
  }
}

assert.deepStrictEqual(new Set(failures.keys()), knownFailures);
