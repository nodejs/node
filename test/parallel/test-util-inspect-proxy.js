// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const util = require('util');
const { internalBinding } = require('internal/test/binding');
const processUtil = internalBinding('util');
const opts = { showProxy: true };

const target = {};
const handler = {
  get: function() { throw new Error('Getter should not be called'); }
};
const proxyObj = new Proxy(target, handler);

// Inspecting the proxy should not actually walk it's properties
util.inspect(proxyObj, opts);

// getProxyDetails is an internal method, not intended for public use.
// This is here to test that the internals are working correctly.
const details = processUtil.getProxyDetails(proxyObj);
assert.strictEqual(target, details[0]);
assert.strictEqual(handler, details[1]);

assert.strictEqual(util.inspect(proxyObj, opts),
                   'Proxy [ {}, { get: [Function: get] } ]');

// Using getProxyDetails with non-proxy returns undefined
assert.strictEqual(processUtil.getProxyDetails({}), undefined);

// This will throw because the showProxy option is not used
// and the get function on the handler object defined above
// is actually invoked.
assert.throws(
  () => util.inspect(proxyObj),
  /^Error: Getter should not be called$/
);

// Yo dawg, I heard you liked Proxy so I put a Proxy
// inside your Proxy that proxies your Proxy's Proxy.
const proxy1 = new Proxy({}, {});
const proxy2 = new Proxy(proxy1, {});
const proxy3 = new Proxy(proxy2, proxy1);
const proxy4 = new Proxy(proxy1, proxy2);
const proxy5 = new Proxy(proxy3, proxy4);
const proxy6 = new Proxy(proxy5, proxy5);
const expected0 = '{}';
const expected1 = 'Proxy [ {}, {} ]';
const expected2 = 'Proxy [ Proxy [ {}, {} ], {} ]';
const expected3 = 'Proxy [ Proxy [ Proxy [ {}, {} ], {} ], Proxy [ {}, {} ] ]';
const expected4 = 'Proxy [ Proxy [ {}, {} ], Proxy [ Proxy [ {}, {} ], {} ] ]';
const expected5 = 'Proxy [ Proxy [ Proxy [ Proxy [Array], {} ],' +
                  ' Proxy [ {}, {} ] ],\n  Proxy [ Proxy [ {}, {} ]' +
                  ', Proxy [ Proxy [Array], {} ] ] ]';
const expected6 = 'Proxy [ Proxy [ Proxy [ Proxy [Array], Proxy [Array]' +
                  ' ],\n    Proxy [ Proxy [Array], Proxy [Array] ] ],\n' +
                  '  Proxy [ Proxy [ Proxy [Array], Proxy [Array] ],\n' +
                  '    Proxy [ Proxy [Array], Proxy [Array] ] ] ]';
assert.strictEqual(
  util.inspect(proxy1, { showProxy: true, depth: null }),
  expected1);
assert.strictEqual(util.inspect(proxy2, opts), expected2);
assert.strictEqual(util.inspect(proxy3, opts), expected3);
assert.strictEqual(util.inspect(proxy4, opts), expected4);
assert.strictEqual(util.inspect(proxy5, opts), expected5);
assert.strictEqual(util.inspect(proxy6, opts), expected6);
assert.strictEqual(util.inspect(proxy1), expected0);
assert.strictEqual(util.inspect(proxy2), expected0);
assert.strictEqual(util.inspect(proxy3), expected0);
assert.strictEqual(util.inspect(proxy4), expected0);
assert.strictEqual(util.inspect(proxy5), expected0);
assert.strictEqual(util.inspect(proxy6), expected0);

// Just for fun, let's create a Proxy using Arrays.
const proxy7 = new Proxy([], []);
const expected7 = 'Proxy [ [], [] ]';
assert.strictEqual(util.inspect(proxy7, opts), expected7);
assert.strictEqual(util.inspect(proxy7), '[]');

// Now we're just getting silly, right?
const proxy8 = new Proxy(Date, []);
const proxy9 = new Proxy(Date, String);
const expected8 = 'Proxy [ [Function: Date], [] ]';
const expected9 = 'Proxy [ [Function: Date], [Function: String] ]';
assert.strictEqual(util.inspect(proxy8, opts), expected8);
assert.strictEqual(util.inspect(proxy9, opts), expected9);
assert.strictEqual(util.inspect(proxy8), '[Function: Date]');
assert.strictEqual(util.inspect(proxy9), '[Function: Date]');

const proxy10 = new Proxy(() => {}, {});
const proxy11 = new Proxy(() => {}, {
  get() {
    return proxy11;
  },
  apply() {
    return proxy11;
  }
});
const expected10 = '[Function]';
const expected11 = '[Function]';
assert.strictEqual(util.inspect(proxy10), expected10);
assert.strictEqual(util.inspect(proxy11), expected11);
