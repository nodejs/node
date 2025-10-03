// Flags: --expose-internals
'use strict';

// Tests the internal utility functions that are used to prepare headers
// to pass to the internal binding layer and to build a header object.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const {
  getAuthority,
  buildNgHeaderString,
  toHeaderObject
} = require('internal/http2/util');
const { sensitiveHeaders } = require('http2');
const { internalBinding } = require('internal/test/binding');
const {
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS,
  HTTP2_HEADER_ACCESS_CONTROL_MAX_AGE,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_METHOD,
  HTTP2_HEADER_AGE,
  HTTP2_HEADER_AUTHORIZATION,
  HTTP2_HEADER_CONTENT_ENCODING,
  HTTP2_HEADER_CONTENT_LANGUAGE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_CONTENT_LOCATION,
  HTTP2_HEADER_CONTENT_MD5,
  HTTP2_HEADER_CONTENT_RANGE,
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_DNT,
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_HOST,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_LOCATION,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_TK,
  HTTP2_HEADER_UPGRADE_INSECURE_REQUESTS,
  HTTP2_HEADER_USER_AGENT,
  HTTP2_HEADER_X_CONTENT_TYPE_OPTIONS,

  HTTP2_HEADER_ACCEPT_CHARSET,
  HTTP2_HEADER_ACCEPT_ENCODING,
  HTTP2_HEADER_ACCEPT_LANGUAGE,
  HTTP2_HEADER_ACCEPT_RANGES,
  HTTP2_HEADER_ACCEPT,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_HEADERS,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_METHODS,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
  HTTP2_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_HEADERS,
  HTTP2_HEADER_ALLOW,
  HTTP2_HEADER_CACHE_CONTROL,
  HTTP2_HEADER_CONTENT_DISPOSITION,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_EXPECT,
  HTTP2_HEADER_FORWARDED,
  HTTP2_HEADER_LINK,
  HTTP2_HEADER_PREFER,
  HTTP2_HEADER_PROXY_AUTHENTICATE,
  HTTP2_HEADER_REFRESH,
  HTTP2_HEADER_SERVER,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_STRICT_TRANSPORT_SECURITY,
  HTTP2_HEADER_TRAILER,
  HTTP2_HEADER_VARY,
  HTTP2_HEADER_VIA,
  HTTP2_HEADER_WARNING,
  HTTP2_HEADER_WWW_AUTHENTICATE,
  HTTP2_HEADER_X_FRAME_OPTIONS,

  HTTP2_HEADER_CONNECTION,
  HTTP2_HEADER_UPGRADE,
  HTTP2_HEADER_HTTP2_SETTINGS,
  HTTP2_HEADER_TE,
  HTTP2_HEADER_TRANSFER_ENCODING,
  HTTP2_HEADER_KEEP_ALIVE,
  HTTP2_HEADER_PROXY_CONNECTION
} = internalBinding('http2').constants;

{
  const headers = {
    'abc': 1,
    ':path': 'abc',
    ':status': 200,
    'xyz': [1, '2', { toString() { return '3'; } }, 4],
    'foo': [],
    'BAR': [1]
  };

  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ [ ':path', 'abc\0', ':status', '200\0', 'abc', '1\0', 'xyz', '1\0',
        'xyz', '2\0', 'xyz', '3\0', 'xyz', '4\0', 'bar', '1\0', '' ].join('\0'),
      8 ]
  );
}

{
  const headers = {
    'abc': 1,
    ':status': [200],
    ':path': 'abc',
    ':authority': [],
    'xyz': [1, 2, 3, 4]
  };

  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ [ ':status', '200\0', ':path', 'abc\0', 'abc', '1\0', 'xyz', '1\0',
        'xyz', '2\0', 'xyz', '3\0', 'xyz', '4\0', '' ].join('\0'), 7 ]
  );
}

{
  const headers = {
    'abc': 1,
    ':status': 200,
    'xyz': [1, 2, 3, 4],
    '': 1,
    ':path': 'abc',
    [Symbol('test')]: 1 // Symbol keys are ignored
  };

  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ [ ':status', '200\0', ':path', 'abc\0', 'abc', '1\0', 'xyz', '1\0',
        'xyz', '2\0', 'xyz', '3\0', 'xyz', '4\0', '' ].join('\0'), 7 ]
  );
}

{
  // Only own properties are used
  const base = { 'abc': 1 };
  const headers = { __proto__: base };
  headers[':status'] = 200;
  headers.xyz = [1, 2, 3, 4];
  headers.foo = [];
  headers[':path'] = 'abc';

  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ [ ':status', '200\0', ':path', 'abc\0', 'xyz', '1\0', 'xyz', '2\0',
        'xyz', '3\0', 'xyz', '4\0', '' ].join('\0'), 6 ]
  );
}

{
  // Arrays containing a single set-cookie value are handled correctly
  // (https://github.com/nodejs/node/issues/16452)
  const headers = {
    'set-cookie': ['foo=bar']
  };
  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ [ 'set-cookie', 'foo=bar\0', '' ].join('\0'), 1 ]
  );
}

{
  // pseudo-headers are only allowed a single value
  const headers = {
    ':status': 200,
    ':statuS': 204,
  };

  assert.throws(() => buildNgHeaderString(headers), {
    code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
    name: 'TypeError',
    message: 'Header field ":status" must only have a single value'
  });
}

{
  const headers = {
    'abc': 1,
    ':status': [200],
    ':path': 'abc',
    ':authority': [],
    'xyz': [1, 2, 3, 4],
    [sensitiveHeaders]: ['xyz']
  };

  assert.deepStrictEqual(
    buildNgHeaderString(headers),
    [ ':status\x00200\x00\x00:path\x00abc\x00\x00abc\x001\x00\x00' +
      'xyz\x001\x00\x01xyz\x002\x00\x01xyz\x003\x00\x01xyz\x004\x00\x01', 7 ]
  );
}

// The following are not allowed to have multiple values
[
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_CREDENTIALS,
  HTTP2_HEADER_ACCESS_CONTROL_MAX_AGE,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_METHOD,
  HTTP2_HEADER_AGE,
  HTTP2_HEADER_AUTHORIZATION,
  HTTP2_HEADER_CONTENT_ENCODING,
  HTTP2_HEADER_CONTENT_LANGUAGE,
  HTTP2_HEADER_CONTENT_LENGTH,
  HTTP2_HEADER_CONTENT_LOCATION,
  HTTP2_HEADER_CONTENT_MD5,
  HTTP2_HEADER_CONTENT_RANGE,
  HTTP2_HEADER_CONTENT_TYPE,
  HTTP2_HEADER_DATE,
  HTTP2_HEADER_DNT,
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_HOST,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_LOCATION,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_TK,
  HTTP2_HEADER_UPGRADE_INSECURE_REQUESTS,
  HTTP2_HEADER_USER_AGENT,
  HTTP2_HEADER_X_CONTENT_TYPE_OPTIONS,
].forEach((name) => {
  const msg = `Header field "${name}" must only have a single value`;
  assert.throws(() => buildNgHeaderString({ [name]: [1, 2, 3] }), {
    code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
    message: msg
  });
});

[
  HTTP2_HEADER_ACCEPT_CHARSET,
  HTTP2_HEADER_ACCEPT_ENCODING,
  HTTP2_HEADER_ACCEPT_LANGUAGE,
  HTTP2_HEADER_ACCEPT_RANGES,
  HTTP2_HEADER_ACCEPT,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_HEADERS,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_METHODS,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
  HTTP2_HEADER_ACCESS_CONTROL_EXPOSE_HEADERS,
  HTTP2_HEADER_ACCESS_CONTROL_REQUEST_HEADERS,
  HTTP2_HEADER_ALLOW,
  HTTP2_HEADER_CACHE_CONTROL,
  HTTP2_HEADER_CONTENT_DISPOSITION,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_EXPECT,
  HTTP2_HEADER_FORWARDED,
  HTTP2_HEADER_LINK,
  HTTP2_HEADER_PREFER,
  HTTP2_HEADER_PROXY_AUTHENTICATE,
  HTTP2_HEADER_REFRESH,
  HTTP2_HEADER_SERVER,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_STRICT_TRANSPORT_SECURITY,
  HTTP2_HEADER_TRAILER,
  HTTP2_HEADER_VARY,
  HTTP2_HEADER_VIA,
  HTTP2_HEADER_WARNING,
  HTTP2_HEADER_WWW_AUTHENTICATE,
  HTTP2_HEADER_X_FRAME_OPTIONS,
].forEach((name) => {
  assert(!(buildNgHeaderString({ [name]: [1, 2, 3] }) instanceof Error), name);
});

[
  HTTP2_HEADER_CONNECTION,
  HTTP2_HEADER_UPGRADE,
  HTTP2_HEADER_HTTP2_SETTINGS,
  HTTP2_HEADER_TE,
  HTTP2_HEADER_TRANSFER_ENCODING,
  HTTP2_HEADER_PROXY_CONNECTION,
  HTTP2_HEADER_KEEP_ALIVE,
  'Connection',
  'Upgrade',
  'HTTP2-Settings',
  'TE',
  'Transfer-Encoding',
  'Proxy-Connection',
  'Keep-Alive',
].forEach((name) => {
  assert.throws(() => buildNgHeaderString({ [name]: 'abc' }), {
    code: 'ERR_HTTP2_INVALID_CONNECTION_HEADERS',
    name: 'TypeError',
    message: 'HTTP/1 Connection specific headers are forbidden: ' +
             `"${name.toLowerCase()}"`
  });
});

assert.throws(() => buildNgHeaderString({ [HTTP2_HEADER_TE]: ['abc'] }), {
  code: 'ERR_HTTP2_INVALID_CONNECTION_HEADERS',
  name: 'TypeError',
  message: 'HTTP/1 Connection specific headers are forbidden: ' +
           `"${HTTP2_HEADER_TE}"`
});

assert.throws(
  () => buildNgHeaderString({ [HTTP2_HEADER_TE]: ['abc', 'trailers'] }), {
    code: 'ERR_HTTP2_INVALID_CONNECTION_HEADERS',
    name: 'TypeError',
    message: 'HTTP/1 Connection specific headers are forbidden: ' +
             `"${HTTP2_HEADER_TE}"`
  });

// These should not throw
buildNgHeaderString({ te: 'trailers' });
buildNgHeaderString({ te: ['trailers'] });

// HTTP/2 encourages use of Host instead of :authority when converting
// from HTTP/1 to HTTP/2, so we no longer disallow it.
// Refs: https://github.com/nodejs/node/issues/29858
buildNgHeaderString({ [HTTP2_HEADER_HOST]: 'abc' });

// If both are present, the latter has priority
assert.strictEqual(getAuthority({
  [HTTP2_HEADER_AUTHORITY]: 'abc',
  [HTTP2_HEADER_HOST]: 'def'
}), 'abc');


{
  const rawHeaders = [
    ':status', '200',
    'cookie', 'foo',
    'set-cookie', 'sc1',
    'age', '10',
    'x-multi', 'first',
  ];
  const headers = toHeaderObject(rawHeaders);
  assert.strictEqual(headers[':status'], 200);
  assert.strictEqual(headers.cookie, 'foo');
  assert.deepStrictEqual(headers['set-cookie'], ['sc1']);
  assert.strictEqual(headers.age, '10');
  assert.strictEqual(headers['x-multi'], 'first');
}

{
  const rawHeaders = [
    ':status', '200',
    ':status', '400',
    'cookie', 'foo',
    'cookie', 'bar',
    'set-cookie', 'sc1',
    'set-cookie', 'sc2',
    'age', '10',
    'age', '20',
    'x-multi', 'first',
    'x-multi', 'second',
  ];
  const headers = toHeaderObject(rawHeaders);
  assert.strictEqual(headers[':status'], 200);
  assert.strictEqual(headers.cookie, 'foo; bar');
  assert.deepStrictEqual(headers['set-cookie'], ['sc1', 'sc2']);
  assert.strictEqual(headers.age, '10');
  assert.strictEqual(headers['x-multi'], 'first, second');
}
