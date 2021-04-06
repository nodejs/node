// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const cares = internalBinding('cares_wrap');
const { promisify } = require('util');

// Test that `dns.setDefaultResultOrder()` and
// `dns.promises.setDefaultResultOrder()` works as expected.

const originalGetaddrinfo = cares.getaddrinfo;
const calls = [];
cares.getaddrinfo = common.mustCallAtLeast((...args) => {
  calls.push(args);
  originalGetaddrinfo(...args);
}, 1);

const dns = require('dns');
const dnsPromises = dns.promises;

let verbatim;

// We want to test the parameter of verbatim only so that we
// ignore possible errors here.
function allowFailed(fn) {
  return fn.catch((_err) => {
    //
  });
}

assert.throws(() => dns.setDefaultResultOrder('my_order'), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => dns.promises.setDefaultResultOrder('my_order'), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => dns.setDefaultResultOrder(4), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => dns.promises.setDefaultResultOrder(4), {
  code: 'ERR_INVALID_ARG_VALUE',
});

(async () => {
  let callsLength = 0;
  const checkParameter = (expected) => {
    assert.strictEqual(calls.length, callsLength + 1);
    verbatim = calls[callsLength][4];
    assert.strictEqual(verbatim, expected);
    callsLength += 1;
  };

  dns.setDefaultResultOrder('verbatim');
  await allowFailed(promisify(dns.lookup)('example.org'));
  checkParameter(true);
  await allowFailed(dnsPromises.lookup('example.org'));
  checkParameter(true);
  await allowFailed(promisify(dns.lookup)('example.org', {}));
  checkParameter(true);
  await allowFailed(dnsPromises.lookup('example.org', {}));
  checkParameter(true);

  dns.setDefaultResultOrder('ipv4first');
  await allowFailed(promisify(dns.lookup)('example.org'));
  checkParameter(false);
  await allowFailed(dnsPromises.lookup('example.org'));
  checkParameter(false);
  await allowFailed(promisify(dns.lookup)('example.org', {}));
  checkParameter(false);
  await allowFailed(dnsPromises.lookup('example.org', {}));
  checkParameter(false);

  dns.promises.setDefaultResultOrder('verbatim');
  await allowFailed(promisify(dns.lookup)('example.org'));
  checkParameter(true);
  await allowFailed(dnsPromises.lookup('example.org'));
  checkParameter(true);
  await allowFailed(promisify(dns.lookup)('example.org', {}));
  checkParameter(true);
  await allowFailed(dnsPromises.lookup('example.org', {}));
  checkParameter(true);

  dns.promises.setDefaultResultOrder('ipv4first');
  await allowFailed(promisify(dns.lookup)('example.org'));
  checkParameter(false);
  await allowFailed(dnsPromises.lookup('example.org'));
  checkParameter(false);
  await allowFailed(promisify(dns.lookup)('example.org', {}));
  checkParameter(false);
  await allowFailed(dnsPromises.lookup('example.org', {}));
  checkParameter(false);
})().then(common.mustCall());
