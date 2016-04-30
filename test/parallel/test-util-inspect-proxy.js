'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const processUtil = process.binding('util');

const target = {};
const handler = {
  get: function() { throw new Error('Getter should not be called'); }
};
const proxyObj = new Proxy(target, handler);

// Inspecting the proxy should not actually walk it's properties
assert.doesNotThrow(() => util.inspect(proxyObj, {showProxy: true}));

// getProxyDetails is an internal method, not intended for public use.
// This is here to test that the internals are working correctly.
const details = processUtil.getProxyDetails(proxyObj);
assert.deepStrictEqual(target, details.target);
assert.deepStrictEqual(handler, details.handler);

assert.strictEqual(util.inspect(proxyObj, {showProxy: true}),
    'Proxy { handler: { get: [Function] }, target: {} }');

// Using getProxyDetails with non-proxy returns undefined
assert.strictEqual(processUtil.getProxyDetails({}), undefined);

// This will throw because the showProxy option is not used
// and the get function on the handler object defined above
// is actually invoked.
assert.throws(
  () => util.inspect(proxyObj)
);
