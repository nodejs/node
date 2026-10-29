'use strict';
const common = require('../common');

// Names containing null bytes must be rejected by the resolver methods.
// c-ares receives the name as a C string, so everything after the first
// null byte would otherwise be silently dropped, e.g. resolving
// 'example.com\0.trusted.corp' would actually query 'example.com'.

const assert = require('assert');
const dns = require('dns');
const dnsPromises = dns.promises;

const name = 'example.com\u0000.trusted.corp';
const ip = '127.0.0.1\u0000.trusted.corp';

const err = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
  message: /^The argument 'name' must be a string without null bytes\. Received /,
};

const methods = [
  'resolve',
  'resolve4',
  'resolve6',
  'resolveAny',
  'resolveCaa',
  'resolveCname',
  'resolveMx',
  'resolveNaptr',
  'resolveNs',
  'resolvePtr',
  'resolveSoa',
  'resolveSrv',
  'resolveTlsa',
  'resolveTxt',
];

const rrtypes = [
  'A',
  'AAAA',
  'ANY',
  'CAA',
  'CNAME',
  'MX',
  'NAPTR',
  'NS',
  'PTR',
  'SOA',
  'SRV',
  'TLSA',
  'TXT',
];

// Callback-based API: the default resolver and a Resolver instance.
for (const resolver of [dns, new dns.Resolver()]) {
  for (const method of methods) {
    assert.throws(() => resolver[method](name, common.mustNotCall()), err);
  }
  for (const rrtype of rrtypes) {
    assert.throws(() => resolver.resolve(name, rrtype, common.mustNotCall()), err);
  }
  assert.throws(() => resolver.reverse(ip, common.mustNotCall()), err);
}

// Promise-based API: the default resolver and a Resolver instance.
for (const resolver of [dnsPromises, new dnsPromises.Resolver()]) {
  for (const method of methods) {
    assert.throws(() => resolver[method](name), err);
  }
  for (const rrtype of rrtypes) {
    assert.throws(() => resolver.resolve(name, rrtype), err);
  }
  assert.throws(() => resolver.reverse(ip), err);
}
