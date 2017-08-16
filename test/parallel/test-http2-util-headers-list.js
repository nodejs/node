// Flags: --expose-internals --expose-http2
'use strict';

// Tests the internal utility function that is used to prepare headers
// to pass to the internal binding layer.

const common = require('../common');
const assert = require('assert');
const { mapToHeaders } = require('internal/http2/util');

const {
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
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
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_USER_AGENT,

  HTTP2_HEADER_ACCEPT_CHARSET,
  HTTP2_HEADER_ACCEPT_ENCODING,
  HTTP2_HEADER_ACCEPT_LANGUAGE,
  HTTP2_HEADER_ACCEPT_RANGES,
  HTTP2_HEADER_ACCEPT,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
  HTTP2_HEADER_ALLOW,
  HTTP2_HEADER_CACHE_CONTROL,
  HTTP2_HEADER_CONTENT_DISPOSITION,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_EXPECT,
  HTTP2_HEADER_LINK,
  HTTP2_HEADER_PREFER,
  HTTP2_HEADER_PROXY_AUTHENTICATE,
  HTTP2_HEADER_REFRESH,
  HTTP2_HEADER_SERVER,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_STRICT_TRANSPORT_SECURITY,
  HTTP2_HEADER_VARY,
  HTTP2_HEADER_VIA,
  HTTP2_HEADER_WWW_AUTHENTICATE,

  HTTP2_HEADER_CONNECTION,
  HTTP2_HEADER_UPGRADE,
  HTTP2_HEADER_HTTP2_SETTINGS,
  HTTP2_HEADER_TE,
  HTTP2_HEADER_TRANSFER_ENCODING,
  HTTP2_HEADER_HOST,
  HTTP2_HEADER_KEEP_ALIVE,
  HTTP2_HEADER_PROXY_CONNECTION
} = process.binding('http2').constants;

{
  const headers = {
    'abc': 1,
    ':status': 200,
    ':path': 'abc',
    'xyz': [1, '2', { toString() { return '3'; } }, 4],
    'foo': [],
    'BAR': [1]
  };

  assert.deepStrictEqual(
    mapToHeaders(headers),
    [ [ ':path', 'abc', ':status', '200', 'abc', '1', 'xyz', '1', 'xyz', '2',
        'xyz', '3', 'xyz', '4', 'bar', '1', '' ].join('\0'), 8 ]
  );
}

{
  const headers = {
    'abc': 1,
    ':path': 'abc',
    ':status': [200],
    ':authority': [],
    'xyz': [1, 2, 3, 4]
  };

  assert.deepStrictEqual(
    mapToHeaders(headers),
    [ [ ':status', '200', ':path', 'abc', 'abc', '1', 'xyz', '1', 'xyz', '2',
        'xyz', '3', 'xyz', '4', '' ].join('\0'), 7 ]
  );
}

{
  const headers = {
    'abc': 1,
    ':path': 'abc',
    'xyz': [1, 2, 3, 4],
    '': 1,
    ':status': 200,
    [Symbol('test')]: 1 // Symbol keys are ignored
  };

  assert.deepStrictEqual(
    mapToHeaders(headers),
    [ [ ':status', '200', ':path', 'abc', 'abc', '1', 'xyz', '1', 'xyz', '2',
        'xyz', '3', 'xyz', '4', '' ].join('\0'), 7 ]
  );
}

{
  // Only own properties are used
  const base = { 'abc': 1 };
  const headers = Object.create(base);
  headers[':path'] = 'abc';
  headers.xyz = [1, 2, 3, 4];
  headers.foo = [];
  headers[':status'] = 200;

  assert.deepStrictEqual(
    mapToHeaders(headers),
    [ [ ':status', '200', ':path', 'abc', 'xyz', '1', 'xyz', '2', 'xyz', '3',
        'xyz', '4', '' ].join('\0'), 6 ]
  );
}

// The following are not allowed to have multiple values
[
  HTTP2_HEADER_STATUS,
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
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
  HTTP2_HEADER_ETAG,
  HTTP2_HEADER_EXPIRES,
  HTTP2_HEADER_FROM,
  HTTP2_HEADER_IF_MATCH,
  HTTP2_HEADER_IF_MODIFIED_SINCE,
  HTTP2_HEADER_IF_NONE_MATCH,
  HTTP2_HEADER_IF_RANGE,
  HTTP2_HEADER_IF_UNMODIFIED_SINCE,
  HTTP2_HEADER_LAST_MODIFIED,
  HTTP2_HEADER_MAX_FORWARDS,
  HTTP2_HEADER_PROXY_AUTHORIZATION,
  HTTP2_HEADER_RANGE,
  HTTP2_HEADER_REFERER,
  HTTP2_HEADER_RETRY_AFTER,
  HTTP2_HEADER_USER_AGENT
].forEach((name) => {
  const msg = `Header field "${name}" must have only a single value`;
  common.expectsError({
    code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
    message: msg
  })(mapToHeaders({ [name]: [1, 2, 3] }));
});

[
  HTTP2_HEADER_ACCEPT_CHARSET,
  HTTP2_HEADER_ACCEPT_ENCODING,
  HTTP2_HEADER_ACCEPT_LANGUAGE,
  HTTP2_HEADER_ACCEPT_RANGES,
  HTTP2_HEADER_ACCEPT,
  HTTP2_HEADER_ACCESS_CONTROL_ALLOW_ORIGIN,
  HTTP2_HEADER_ALLOW,
  HTTP2_HEADER_CACHE_CONTROL,
  HTTP2_HEADER_CONTENT_DISPOSITION,
  HTTP2_HEADER_COOKIE,
  HTTP2_HEADER_EXPECT,
  HTTP2_HEADER_LINK,
  HTTP2_HEADER_PREFER,
  HTTP2_HEADER_PROXY_AUTHENTICATE,
  HTTP2_HEADER_REFRESH,
  HTTP2_HEADER_SERVER,
  HTTP2_HEADER_SET_COOKIE,
  HTTP2_HEADER_STRICT_TRANSPORT_SECURITY,
  HTTP2_HEADER_VARY,
  HTTP2_HEADER_VIA,
  HTTP2_HEADER_WWW_AUTHENTICATE
].forEach((name) => {
  assert(!(mapToHeaders({ [name]: [1, 2, 3] }) instanceof Error), name);
});

const regex =
  /^HTTP\/1 Connection specific headers are forbidden$/;
[
  HTTP2_HEADER_CONNECTION,
  HTTP2_HEADER_UPGRADE,
  HTTP2_HEADER_HTTP2_SETTINGS,
  HTTP2_HEADER_TE,
  HTTP2_HEADER_TRANSFER_ENCODING,
  HTTP2_HEADER_HOST,
  HTTP2_HEADER_PROXY_CONNECTION,
  HTTP2_HEADER_KEEP_ALIVE,
  'Connection',
  'Upgrade',
  'HTTP2-Settings',
  'TE',
  'Transfer-Encoding',
  'Proxy-Connection',
  'Keep-Alive'
].forEach((name) => {
  common.expectsError({
    code: 'ERR_HTTP2_INVALID_CONNECTION_HEADERS',
    message: regex
  })(mapToHeaders({ [name]: 'abc' }));
});

assert(!(mapToHeaders({ te: 'trailers' }) instanceof Error));
