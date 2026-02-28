// Flags: --async-context-frame
'use strict';

require('../common');

const {
  AsyncLocalStorage,
} = require('async_hooks');

const assert = require('assert');

// ============================================================================
// The defaultValue option
const als1 = new AsyncLocalStorage();
assert.strictEqual(als1.getStore(), undefined);

const als2 = new AsyncLocalStorage({ defaultValue: 'default' });
assert.strictEqual(als2.getStore(), 'default');

const als3 = new AsyncLocalStorage({ defaultValue: 42 });
assert.strictEqual(als3.getStore(), 42);

const als4 = new AsyncLocalStorage({ defaultValue: null });
assert.strictEqual(als4.getStore(), null);

assert.throws(() => new AsyncLocalStorage(null), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// ============================================================================
// The name option

const als5 = new AsyncLocalStorage({ name: 'test' });
assert.strictEqual(als5.name, 'test');

const als6 = new AsyncLocalStorage();
assert.strictEqual(als6.name, '');
