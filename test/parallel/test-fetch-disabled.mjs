import '../common/index.mjs';

import assert from 'assert';

assert.strictEqual(typeof globalThis.fetch, 'undefined');
assert.strictEqual(typeof globalThis.FormData, 'undefined');
assert.strictEqual(typeof globalThis.Headers, 'undefined');
assert.strictEqual(typeof globalThis.Request, 'undefined');
assert.strictEqual(typeof globalThis.Response, 'undefined');
