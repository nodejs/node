'use strict';
require('../common');
const assert = require('assert');
const { urlToHttpOptions } = require('url');

// Test urlToHttpOptions
const urlObj = new URL('http://user:pass@foo.bar.com:21/aaa/zzz?l=24#test');
const opts = urlToHttpOptions(urlObj);
assert.strictEqual(opts instanceof URL, false);
assert.strictEqual(opts.protocol, 'http:');
assert.strictEqual(opts.auth, 'user:pass');
assert.strictEqual(opts.hostname, 'foo.bar.com');
assert.strictEqual(opts.port, 21);
assert.strictEqual(opts.path, '/aaa/zzz?l=24');
assert.strictEqual(opts.pathname, '/aaa/zzz');
assert.strictEqual(opts.search, '?l=24');
assert.strictEqual(opts.hash, '#test');

const { hostname } = urlToHttpOptions(new URL('http://[::1]:21'));
assert.strictEqual(hostname, '::1');

// If a WHATWG URL object is copied, it is possible that the resulting copy
// contains the Symbols that Node uses for brand checking, but not the data
// properties, which are getters. Verify that urlToHttpOptions() can handle
// such a case.
const copiedUrlObj = { ...urlObj };
const copiedOpts = urlToHttpOptions(copiedUrlObj);
assert.strictEqual(copiedOpts instanceof URL, false);
assert.strictEqual(copiedOpts.protocol, undefined);
assert.strictEqual(copiedOpts.auth, undefined);
assert.strictEqual(copiedOpts.hostname, undefined);
assert.strictEqual(copiedOpts.port, NaN);
assert.strictEqual(copiedOpts.path, '');
assert.strictEqual(copiedOpts.pathname, undefined);
assert.strictEqual(copiedOpts.search, undefined);
assert.strictEqual(copiedOpts.hash, undefined);
assert.strictEqual(copiedOpts.href, undefined);

// A genuine URL instance can still be extended with own enumerable properties
// (e.g. extra request options) and those properties are carried through.
const extendedUrl = new URL('http://user:pass@foo.bar.com:21/aaa/zzz?l=24#test');
extendedUrl.headers = { foo: 'Bar' };
extendedUrl.method = 'POST';
const extendedOpts = urlToHttpOptions(extendedUrl);
assert.deepStrictEqual(extendedOpts.headers, { foo: 'Bar' });
assert.strictEqual(extendedOpts.method, 'POST');

// A duck-typed object that satisfies `isURL()` (truthy `href` and `protocol`,
// `auth`/`path` absent) must not be able to smuggle transport/connection
// options (e.g. `socketPath`) into the request options. Only genuine WHATWG
// URL instances may carry additional own enumerable properties.
const duckTypedUrl = {
  href: 'http://safe-webhook.com/verify',
  protocol: 'http:',
  hostname: 'safe-webhook.com',
  pathname: '/v1.41/version',
  port: '',
  username: '',
  password: '',
  search: '',
  hash: '',
  socketPath: '/var/run/docker.sock',
  method: 'GET',
  headers: { 'Content-Type': 'application/json' },
  agent: false,
};
const duckTypedOpts = urlToHttpOptions(duckTypedUrl);
assert.strictEqual(duckTypedOpts.socketPath, undefined);
assert.strictEqual(duckTypedOpts.method, undefined);
assert.strictEqual(duckTypedOpts.headers, undefined);
assert.strictEqual(duckTypedOpts.agent, undefined);
// URL-derived fields are still populated from the duck-typed object.
assert.strictEqual(duckTypedOpts.hostname, 'safe-webhook.com');
assert.strictEqual(duckTypedOpts.path, '/v1.41/version');
assert.strictEqual(duckTypedOpts.protocol, 'http:');

// Test when passed an invalid argument
assert.throws(() => {
  urlToHttpOptions('http://127.0.0.1');
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  message: 'The "url" argument must be of type object. Received type string (\'http://127.0.0.1\')',
  name: 'TypeError'
});
