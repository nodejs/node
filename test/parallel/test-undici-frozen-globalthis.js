'use strict';

// Test that globals provided by undici (fetch, WebSocket, etc.) do not throw
// when globalThis has been frozen via Object.freeze(globalThis).
//
// Refs: https://github.com/nodejs/node/issues/46788

require('../common');

// Freeze globalThis before any undici globals are accessed.
Object.freeze(globalThis);

const assert = require('node:assert');

// Accessing these globals triggers undici's lazy initialisation, which calls
// setGlobalDispatcher internally. With a frozen globalThis, the original code
// threw: "TypeError: Cannot define property Symbol(undici.globalDispatcher.2),
// object is not extensible". The fix uses a module-level fallback instead.
assert.strictEqual(typeof fetch, 'function', 'fetch must be a function');
assert.strictEqual(typeof WebSocket, 'function', 'WebSocket must be a function');
assert.strictEqual(typeof Response, 'function', 'Response must be a function');
assert.strictEqual(typeof Request, 'function', 'Request must be a function');
assert.strictEqual(typeof Headers, 'function', 'Headers must be a function');
assert.strictEqual(typeof FormData, 'function', 'FormData must be a function');

console.log('All undici globals accessible after Object.freeze(globalThis)');
