// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const { dnsException } = require('internal/errors');
const { internalBinding } = require('internal/test/binding');
const { getaddrinfo, GetAddrInfoReqWrap } = internalBinding('cares_wrap');
const dns = require('dns');

// Stub `getaddrinfo` to *always* error.

// GetAddrInfoReqWrap in "reqWrap" after calling dns.lookup();
const reqWrap = dns.lookup('127.0.0.1', {
  hints: 0,
  family: 4,
  all: false
}, common.mustCall((error, result, addressType) => {
  assert.ifError(error);
  assert.deepStrictEqual(result, '127.0.0.1');
  assert.strictEqual(addressType, 4);
}));

let tickValue = 0;

// use reqWrap.onlookup for req.oncomplete of GetAddrInfoReqWrap
// reqWrap.onlookup must return response in promises
new Promise((resolve, reject) => {
  const req = new GetAddrInfoReqWrap();

  req.family = 0;
  req.hostname = '127.0.0.1';
  req.oncomplete = reqWrap.onlookup;
  req.resolve = resolve;
  req.reject = reject;
  // const err = getaddrinfo(req, hostname, family, hints, verbatim);
}).then((err, rw) => {
  assert.deepStrictEqual(rw, { address: '127.0.0.1', family: 4 });
  assert.strictEqual(tickValue, 1);
});

new Promise((resolve, reject) => {
  const req = new GetAddrInfoReqWrap();

  req.family = 0;
  req.hostname = '127.0.0.1';
  req.oncomplete = reqWrap.onlookup;
  req.resolve = resolve;
  req.reject = reject;

  const hints = 0;
  const family = 0;
  const verbatim = false;
  const hostname = '127.0.0.1';

  const err = getaddrinfo(req, hostname, family, hints, verbatim);
  if (err) {
    reject(dnsException(err, 'getaddrinfo', hostname));
  }

}).then((err, rw) => {
  assert.deepStrictEqual(rw, { address: '127.0.0.1', family: 4 });
  assert.ifError(err);
  assert.strictEqual(tickValue, 1);
});

// Make sure that the Promise is returned
// on next tick.
tickValue = 1;
