// Flags: --expose-internals
'use strict';

const common = require('../common');
const { internalBinding } = require('internal/test/binding');
const providers = internalBinding('async_wrap').Providers;
const assert = require('assert');
const { asyncWrapProviders } = require('async_hooks');

assert.ok(typeof asyncWrapProviders === 'object');
assert.deepStrictEqual(asyncWrapProviders, { __proto__: null, ...providers });

const providerKeys = Object.keys(asyncWrapProviders);
assert.throws(() => {
  asyncWrapProviders[providerKeys[0]] = 'another value';
}, common.expectsError({
  name: 'TypeError',
}), 'should not allow modify asyncWrap providers');
