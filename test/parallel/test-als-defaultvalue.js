// Flags: --async-context-frame
'use strict';

require('../common');

const {
  AsyncLocalStorage,
} = require('async_hooks');

const {
  strictEqual,
  throws,
} = require('assert');

// ============================================================================
// The defaultValue option
const als1 = new AsyncLocalStorage();
strictEqual(als1.getStore(), undefined, 'value should be undefined');

const als2 = new AsyncLocalStorage({ defaultValue: 'default' });
strictEqual(als2.getStore(), 'default', 'value should be "default"');

const als3 = new AsyncLocalStorage({ defaultValue: 42 });
strictEqual(als3.getStore(), 42, 'value should be 42');

const als4 = new AsyncLocalStorage({ defaultValue: null });
strictEqual(als4.getStore(), null, 'value should be null');

throws(() => new AsyncLocalStorage(null), {
  code: 'ERR_INVALID_ARG_TYPE',
});

// ============================================================================
// The name option

const als5 = new AsyncLocalStorage({ name: 'test' });
strictEqual(als5.name, 'test');

const als6 = new AsyncLocalStorage();
strictEqual(als6.name, '');
