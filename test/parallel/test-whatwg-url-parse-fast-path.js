'use strict';

// Covers the URL constructor parse paths that avoid a UTF-8 copy and/or
// reuse the input string when it is already a serialized ASCII href.

const { hasIntl } = require('../common');
const assert = require('assert');

const alreadySerialized = [
  'https://nodejs.org/en/blog/',
  'http://nodejs.org:89/docs/latest/api/foo/bar/qua/13949281/0f28b/' +
    '/5d49/b3020/url.html#test?payload1=true&payload2=false&test=1' +
    '&benchmark=3&foo=38.38.011.293&bar=1234834910480&test=19299&3992&' +
    'key=f5c65e1e98fe07e648249ad41e1cfdb0',
  'https://user:pass@example.com/path?search=1',
  'file:///foo/bar/test/node.js',
  'ws://localhost:9229/f46db715-70df-43ad-a359-7f9949f39868',
];

for (const href of alreadySerialized) {
  const url = new URL(href);
  assert.strictEqual(url.href, href);
  assert.strictEqual(URL.parse(href).href, href);
  assert.strictEqual(URL.canParse(href), true);
}

// Special-scheme URLs with an empty path gain a trailing slash.
{
  const url = new URL('https://example.com');
  assert.strictEqual(url.href, 'https://example.com/');
  assert.strictEqual(url.pathname, '/');
}

// Dot-segment normalization must still rewrite the path.
{
  const url = new URL('https://example.org/./a/../b/./c');
  assert.strictEqual(url.href, 'https://example.org/b/c');
  assert.strictEqual(url.pathname, '/b/c');
}

// Relative resolution against a base URL.
{
  const url = new URL('/path?x=1#h', 'https://example.com:8443/base');
  assert.strictEqual(url.href, 'https://example.com:8443/path?x=1#h');
  assert.strictEqual(url.host, 'example.com:8443');
}

// Non-string input is still stringified.
{
  const url = new URL({ toString: () => 'https://example.com/from-object' });
  assert.strictEqual(url.href, 'https://example.com/from-object');
}

// Invalid input still throws from the constructor and is null from parse().
{
  assert.throws(() => new URL('not a url'), {
    code: 'ERR_INVALID_URL',
    name: 'TypeError',
  });
  assert.strictEqual(URL.parse('not a url'), null);
  assert.strictEqual(URL.canParse('not a url'), false);
}

// Unpaired surrogates must not be returned as-is from href.
{
  const input = 'https://example.com/\uD800';
  const url = new URL(input);
  assert.notStrictEqual(url.href, input);
  assert.ok(url.href.startsWith('https://example.com/'));
}

if (hasIntl) {
  const url = new URL('http://你好你好.在线');
  assert.ok(url.hostname.startsWith('xn--'));
  assert.ok(url.href.startsWith('http://xn--'));
}

// Setters re-parse the existing href; keep component updates correct.
{
  const url = new URL('https://example.com/old');
  url.pathname = '/new';
  url.search = 'q=1';
  url.hash = 'frag';
  assert.strictEqual(url.href, 'https://example.com/new?q=1#frag');
  assert.strictEqual(url.pathname, '/new');
  assert.strictEqual(url.search, '?q=1');
  assert.strictEqual(url.hash, '#frag');
}
