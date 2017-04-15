// Flags: --expose-internals
'use strict';

require('../common');
const URL = require('url').URL;
const assert = require('assert');
const urlToOptions = require('internal/url').urlToOptions;

// Tests below are not from WPT.
const url = new URL('http://user:pass@foo.bar.com:21/aaa/zzz?l=24#test');
const oldParams = url.searchParams;  // for test of [SameObject]

// To retrieve enumerable but not necessarily own properties,
// we need to use the for-in loop.
const props = [];
for (const prop in url) {
  props.push(prop);
}

// See: https://url.spec.whatwg.org/#api
// https://heycam.github.io/webidl/#es-attributes
// https://heycam.github.io/webidl/#es-stringifier
const expected = ['toString',
                  'href', 'origin', 'protocol',
                  'username', 'password', 'host', 'hostname', 'port',
                  'pathname', 'search', 'searchParams', 'hash', 'toJSON'];

assert.deepStrictEqual(props, expected);

// href is writable (not readonly) and is stringifier
assert.strictEqual(url.toString(), url.href);
url.href = 'http://user:pass@foo.bar.com:21/aaa/zzz?l=25#test';
assert.strictEqual(url.href,
                   'http://user:pass@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual(url.toString(), url.href);
// Return true because it's configurable, but because the properties
// are defined on the prototype per the spec, the deletion has no effect
assert.strictEqual((delete url.href), true);
assert.strictEqual(url.href,
                   'http://user:pass@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual(url.searchParams, oldParams);  // [SameObject]

// searchParams is readonly. Under strict mode setting a
// non-writable property should throw.
// Note: this error message is subject to change in V8 updates
assert.throws(() => url.origin = 'http://foo.bar.com:22',
              new RegExp('TypeError: Cannot set property origin of' +
                         ' \\[object URL\\] which has only a getter'));
assert.strictEqual(url.origin, 'http://foo.bar.com:21');
assert.strictEqual(url.toString(),
                   'http://user:pass@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual((delete url.origin), true);
assert.strictEqual(url.origin, 'http://foo.bar.com:21');

// The following properties should be writable (not readonly)
url.protocol = 'https:';
assert.strictEqual(url.protocol, 'https:');
assert.strictEqual(url.toString(),
                   'https://user:pass@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual((delete url.protocol), true);
assert.strictEqual(url.protocol, 'https:');

url.username = 'user2';
assert.strictEqual(url.username, 'user2');
assert.strictEqual(url.toString(),
                   'https://user2:pass@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual((delete url.username), true);
assert.strictEqual(url.username, 'user2');

url.password = 'pass2';
assert.strictEqual(url.password, 'pass2');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.com:21/aaa/zzz?l=25#test');
assert.strictEqual((delete url.password), true);
assert.strictEqual(url.password, 'pass2');

url.host = 'foo.bar.net:22';
assert.strictEqual(url.host, 'foo.bar.net:22');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.net:22/aaa/zzz?l=25#test');
assert.strictEqual((delete url.host), true);
assert.strictEqual(url.host, 'foo.bar.net:22');

url.hostname = 'foo.bar.org';
assert.strictEqual(url.hostname, 'foo.bar.org');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:22/aaa/zzz?l=25#test');
assert.strictEqual((delete url.hostname), true);
assert.strictEqual(url.hostname, 'foo.bar.org');

url.port = '23';
assert.strictEqual(url.port, '23');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:23/aaa/zzz?l=25#test');
assert.strictEqual((delete url.port), true);
assert.strictEqual(url.port, '23');

url.pathname = '/aaa/bbb';
assert.strictEqual(url.pathname, '/aaa/bbb');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:23/aaa/bbb?l=25#test');
assert.strictEqual((delete url.pathname), true);
assert.strictEqual(url.pathname, '/aaa/bbb');

url.search = '?k=99';
assert.strictEqual(url.search, '?k=99');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:23/aaa/bbb?k=99#test');
assert.strictEqual((delete url.search), true);
assert.strictEqual(url.search, '?k=99');

url.hash = '#abcd';
assert.strictEqual(url.hash, '#abcd');
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:23/aaa/bbb?k=99#abcd');
assert.strictEqual((delete url.hash), true);
assert.strictEqual(url.hash, '#abcd');

// searchParams is readonly. Under strict mode setting a
// non-writable property should throw.
// Note: this error message is subject to change in V8 updates
assert.throws(() => url.searchParams = '?k=88',
              new RegExp('TypeError: Cannot set property searchParams of' +
                         ' \\[object URL\\] which has only a getter'));
assert.strictEqual(url.searchParams, oldParams);
assert.strictEqual(url.toString(),
                   'https://user2:pass2@foo.bar.org:23/aaa/bbb?k=99#abcd');
assert.strictEqual((delete url.searchParams), true);
assert.strictEqual(url.searchParams, oldParams);

// Test urlToOptions
{
  const opts =
    urlToOptions(new URL('http://user:pass@foo.bar.com:21/aaa/zzz?l=24#test'));
  assert.strictEqual(opts instanceof URL, false);
  assert.strictEqual(opts.protocol, 'http:');
  assert.strictEqual(opts.auth, 'user:pass');
  assert.strictEqual(opts.hostname, 'foo.bar.com');
  assert.strictEqual(opts.port, 21);
  assert.strictEqual(opts.path, '/aaa/zzz?l=24');
  assert.strictEqual(opts.pathname, '/aaa/zzz');
  assert.strictEqual(opts.search, '?l=24');
  assert.strictEqual(opts.hash, '#test');
}

// Test special origins
[
  { expected: 'https://whatwg.org',
    url: 'blob:https://whatwg.org/d0360e2f-caee-469f-9a2f-87d5b0456f6f' },
  { expected: 'ftp://example.org', url: 'ftp://example.org/foo' },
  { expected: 'gopher://gopher.quux.org', url: 'gopher://gopher.quux.org/1/' },
  { expected: 'http://example.org', url: 'http://example.org/foo' },
  { expected: 'https://example.org', url: 'https://example.org/foo' },
  { expected: 'ws://example.org', url: 'ws://example.org/foo' },
  { expected: 'wss://example.org', url: 'wss://example.org/foo' },
  { expected: 'null', url: 'file:///tmp/mock/path' },
  { expected: 'null', url: 'npm://nodejs/rules' }
].forEach((test) => {
  assert.strictEqual(new URL(test.url).origin, test.expected);
});
