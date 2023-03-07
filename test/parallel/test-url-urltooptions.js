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
