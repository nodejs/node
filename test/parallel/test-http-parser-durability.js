var assert = require('assert');
var inspect = require('util').inspect;

var HTTPParser = require('_http_parser');

var CRLF = '\r\n';
var REQUEST = HTTPParser.REQUEST;
var RESPONSE = HTTPParser.RESPONSE;

var cases = [
  // REQUESTS ==================================================================
  {
    name: 'curl get',
    type: REQUEST,
    raw: [
      'GET /test HTTP/1.1',
      'User-Agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 '
        + 'OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1',
      'Host: 0.0.0.0=5000',
      'Accept: */*',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/test',
    headers: [
      'User-Agent',
        'curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 '
          + 'OpenSSL/0.9.8g zlib/1.2.3.3 libidn/1.1',
      'Host',
        '0.0.0.0=5000',
      'Accept',
        '*/*',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'firefox get',
    type: REQUEST,
    raw: [
      'GET /favicon.ico HTTP/1.1',
      'Host: 0.0.0.0=5000',
      'User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) '
        + 'Gecko/2008061015 Firefox/3.0',
      'Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'Accept-Language: en-us,en;q=0.5',
      'Accept-Encoding: gzip,deflate',
      'Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7',
      'Keep-Alive: 300',
      'Connection: keep-alive',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/favicon.ico',
    headers: [
      'Host',
        '0.0.0.0=5000',
      'User-Agent',
        'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) '
          + 'Gecko/2008061015 Firefox/3.0',
      'Accept',
        'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
      'Accept-Language',
        'en-us,en;q=0.5',
      'Accept-Encoding',
        'gzip,deflate',
      'Accept-Charset',
        'ISO-8859-1,utf-8;q=0.7,*;q=0.7',
      'Keep-Alive',
        '300',
      'Connection',
        'keep-alive',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'repeating characters',
    type: REQUEST,
    raw: [
      'GET /repeater HTTP/1.1',
      'aaaaaaaaaaaaa:++++++++++',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/repeater',
    headers: [
      'aaaaaaaaaaaaa',
        '++++++++++',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'fragment in url',
    type: REQUEST,
    raw: [
      'GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/forums/1/topics/2375?page=1#posts-17408',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'get no headers no body',
    type: REQUEST,
    raw: [
      'GET /get_no_headers_no_body/world HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/get_no_headers_no_body/world',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'get one header no body',
    type: REQUEST,
    raw: [
      'GET /get_one_header_no_body HTTP/1.1',
      'Accept: */*',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/get_one_header_no_body',
    headers: [
      'Accept',
        '*/*',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'get funky content length body hello',
    type: REQUEST,
    raw: [
      'GET /get_funky_content_length_body_hello HTTP/1.0',
      'conTENT-Length: 5',
      '',
      'HELLO'
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    method: 'GET',
    url: '/get_funky_content_length_body_hello',
    headers: [
      'conTENT-Length',
        '5',
    ],
    upgrade: false,
    body: 'HELLO'
  },
  {
    name: 'post identity body world',
    type: REQUEST,
    raw: [
      'POST /post_identity_body_world?q=search#hey HTTP/1.1',
      'Accept: */*',
      'Transfer-Encoding: identity',
      'Content-Length: 5',
      '',
      'World'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/post_identity_body_world?q=search#hey',
    headers: [
      'Accept',
        '*/*',
      'Transfer-Encoding',
        'identity',
      'Content-Length',
        '5',
    ],
    upgrade: false,
    body: 'World'
  },
  {
    name: 'post - chunked body: all your base are belong to us',
    type: REQUEST,
    raw: [
      'POST /post_chunked_all_your_base HTTP/1.1',
      'Transfer-Encoding: chunked',
      '',
      '1e',
      'all your base are belong to us',
      '0',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/post_chunked_all_your_base',
    headers: [
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: 'all your base are belong to us'
  },
  {
    name: 'two chunks ; triple zero ending',
    type: REQUEST,
    raw: [
      'POST /two_chunks_mult_zero_end HTTP/1.1',
      'Transfer-Encoding: chunked',
      '',
      '5',
      'hello',
      '6',
      ' world',
      '000',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/two_chunks_mult_zero_end',
    headers: [
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: 'hello world'
  },
  {
    name: 'chunked with trailing headers',
    type: REQUEST,
    raw: [
      'POST /chunked_w_trailing_headers HTTP/1.1',
      'Transfer-Encoding: chunked',
      '',
      '5',
      'hello',
      '6',
      ' world',
      '0',
      'Vary: *',
      'Content-Type: text/plain',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/chunked_w_trailing_headers',
    headers: [
      'Transfer-Encoding',
        'chunked',
      'Vary',
        '*',
      'Content-Type',
        'text/plain',
    ],
    upgrade: false,
    body: 'hello world'
  },
  {
    name: 'chunked with chunk extensions',
    type: REQUEST,
    raw: [
      'POST /chunked_w_extensions HTTP/1.1',
      'Transfer-Encoding: chunked',
      '',
      '5; woohoo3;whaaaaaaaat=aretheseparametersfor',
      'hello',
      '6; blahblah; blah',
      ' world',
      '0',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/chunked_w_extensions',
    headers: [
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: 'hello world'
  },
  {
    name: 'with quotes',
    type: REQUEST,
    raw: [
      'GET /with_"stupid"_quotes?foo="bar" HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/with_"stupid"_quotes?foo="bar"',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'apachebench get',
    type: REQUEST,
    raw: [
      'GET /test HTTP/1.0',
      'Host: 0.0.0.0:5000',
      'User-Agent: ApacheBench/2.3',
      'Accept: */*',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    method: 'GET',
    url: '/test',
    headers: [
      'Host',
        '0.0.0.0:5000',
      'User-Agent',
        'ApacheBench/2.3',
      'Accept',
        '*/*',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'query url with question mark',
    type: REQUEST,
    raw: [
      'GET /test.cgi?foo=bar?baz HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/test.cgi?foo=bar?baz',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'newline prefix get',
    type: REQUEST,
    raw: [
      '',
      'GET /test HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/test',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'upgrade request',
    type: REQUEST,
    raw: [
      'GET /demo HTTP/1.1',
      'Host: example.com',
      'Connection: Upgrade',
      'Sec-WebSocket-Key2: 12998 5 Y3 1  .P00',
      'Sec-WebSocket-Protocol: sample',
      'Upgrade: WebSocket',
      'Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5',
      'Origin: http://example.com',
      '', '',
      'Hot diggity dogg'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/demo',
    headers: [
      'Host',
        'example.com',
      'Connection',
        'Upgrade',
      'Sec-WebSocket-Key2',
        '12998 5 Y3 1  .P00',
      'Sec-WebSocket-Protocol',
        'sample',
      'Upgrade',
        'WebSocket',
      'Sec-WebSocket-Key1',
        '4 @1  46546xW%0l 1 5',
      'Origin',
        'http://example.com',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'connect request',
    type: REQUEST,
    raw: [
      'CONNECT 0-home0.netscape.com:443 HTTP/1.0',
      'User-agent: Mozilla/1.1N',
      'Proxy-authorization: basic aGVsbG86d29ybGQ=',
      '', '',
      'some data',
      'and yet even more data'
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    method: 'CONNECT',
    url: '0-home0.netscape.com:443',
    headers: [
      'User-agent',
        'Mozilla/1.1N',
      'Proxy-authorization',
        'basic aGVsbG86d29ybGQ=',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'report request',
    type: REQUEST,
    raw: [
      'REPORT /test HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'REPORT',
    url: '/test',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'request with no http version',
    type: REQUEST,
    raw: [
      'GET /',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 0,
    httpMinor: 9,
    method: 'GET',
    url: '/',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'm-search request',
    type: REQUEST,
    raw: [
      'M-SEARCH * HTTP/1.1',
      'HOST: 239.255.255.250:1900',
      'MAN: "ssdp:discover"',
      'ST: "ssdp:all"',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'M-SEARCH',
    url: '*',
    headers: [
      'HOST',
        '239.255.255.250:1900',
      'MAN',
        '"ssdp:discover"',
      'ST',
        '"ssdp:all"',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'line folding in header value',
    type: REQUEST,
    raw: [
      'GET / HTTP/1.1',
      'Line1:   abc',
      '\tdef',
      ' ghi',
      '\t\tjkl',
      '  mno ',
      '\t \tqrs',
      'Line2: \t line2\t',
      'Line3:',
      ' line3',
      'Line4: ',
      ' ',
      'Connection:',
      ' close',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/',
    headers: [
      'Line1',
        'abc def ghi jkl mno  qrs',
      'Line2',
        'line2',
      'Line3',
        'line3',
      'Line4',
        '',
      'Connection',
        'close',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'host terminated by a query string',
    type: REQUEST,
    raw: [
      'GET http://example.org?hail=all HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: 'http://example.org?hail=all',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'host:port terminated by a query string',
    type: REQUEST,
    raw: [
      'GET http://example.org:1234?hail=all HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: 'http://example.org:1234?hail=all',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'host:port terminated by a space',
    type: REQUEST,
    raw: [
      'GET http://example.org:1234 HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: 'http://example.org:1234',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'PATCH request',
    type: REQUEST,
    raw: [
      'PATCH /file.txt HTTP/1.1',
      'Host: www.example.com',
      'Content-Type: application/example',
      'If-Match: "e0023aa4e"',
      'Content-Length: 10',
      '',
      'cccccccccc'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'PATCH',
    url: '/file.txt',
    headers: [
      'Host',
        'www.example.com',
      'Content-Type',
        'application/example',
      'If-Match',
        '"e0023aa4e"',
      'Content-Length',
        '10',
    ],
    upgrade: false,
    body: 'cccccccccc'
  },
  {
    name: 'connect caps request',
    type: REQUEST,
    raw: [
      'CONNECT HOME0.NETSCAPE.COM:443 HTTP/1.0',
      'User-agent: Mozilla/1.1N',
      'Proxy-authorization: basic aGVsbG86d29ybGQ=',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    method: 'CONNECT',
    url: 'HOME0.NETSCAPE.COM:443',
    headers: [
      'User-agent',
        'Mozilla/1.1N',
      'Proxy-authorization',
        'basic aGVsbG86d29ybGQ=',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'utf-8 path request',
    type: REQUEST,
    raw: [
      new Buffer('GET /δ¶/δt/pope?q=1#narf HTTP/1.1', 'utf8')
        .toString('binary'),
      'Host: github.com',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: new Buffer('/δ¶/δt/pope?q=1#narf', 'utf8').toString('binary'),
    headers: [
      'Host',
        'github.com',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'hostname underscore',
    type: REQUEST,
    raw: [
      'CONNECT home_0.netscape.com:443 HTTP/1.0',
      'User-agent: Mozilla/1.1N',
      'Proxy-authorization: basic aGVsbG86d29ybGQ=',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    method: 'CONNECT',
    url: 'home_0.netscape.com:443',
    headers: [
      'User-agent',
        'Mozilla/1.1N',
      'Proxy-authorization',
        'basic aGVsbG86d29ybGQ=',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'eat CRLF between requests, no "Connection: close" header',
    type: REQUEST,
    raw: [
      'POST / HTTP/1.1',
      'Host: www.example.com',
      'Content-Type: application/x-www-form-urlencoded',
      'Content-Length: 4',
      '',
      'q=42',
      ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/',
    headers: [
      'Host',
        'www.example.com',
      'Content-Type',
        'application/x-www-form-urlencoded',
      'Content-Length',
        '4',
    ],
    upgrade: false,
    body: 'q=42'
  },
  {
    name: 'eat CRLF between requests even if "Connection: close" is set',
    type: REQUEST,
    raw: [
      'POST / HTTP/1.1',
      'Host: www.example.com',
      'Content-Type: application/x-www-form-urlencoded',
      'Content-Length: 4',
      'Connection: close',
      '',
      'q=42',
      ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'POST',
    url: '/',
    headers: [
      'Host',
        'www.example.com',
      'Content-Type',
        'application/x-www-form-urlencoded',
      'Content-Length',
        '4',
      'Connection',
        'close',
    ],
    upgrade: false,
    body: 'q=42'
  },
  {
    name: 'PURGE request',
    type: REQUEST,
    raw: [
      'PURGE /file.txt HTTP/1.1',
      'Host: www.example.com',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'PURGE',
    url: '/file.txt',
    headers: [
      'Host',
        'www.example.com',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'SEARCH request',
    type: REQUEST,
    raw: [
      'SEARCH / HTTP/1.1',
      'Host: www.example.com',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'SEARCH',
    url: '/',
    headers: [
      'Host',
        'www.example.com',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'host:port and basic_auth',
    type: REQUEST,
    raw: [
      'GET http://a%12:b!&*$@example.org:1234/toto HTTP/1.1',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: 'http://a%12:b!&*$@example.org:1234/toto',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'multiple connection header values with folding',
    type: REQUEST,
    raw: [
      'GET /demo HTTP/1.1',
      'Host: example.com',
      'Connection: Something,',
      ' Upgrade, ,Keep-Alive',
      'Sec-WebSocket-Key2: 12998 5 Y3 1  .P00',
      'Sec-WebSocket-Protocol: sample',
      'Upgrade: WebSocket',
      'Sec-WebSocket-Key1: 4 @1  46546xW%0l 1 5',
      'Origin: http://example.com',
      '', '',
      'Hot diggity dogg'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/demo',
    headers: [
      'Host',
        'example.com',
      'Connection',
        'Something, Upgrade, ,Keep-Alive',
      'Sec-WebSocket-Key2',
        '12998 5 Y3 1  .P00',
      'Sec-WebSocket-Protocol',
        'sample',
      'Upgrade',
        'WebSocket',
      'Sec-WebSocket-Key1',
        '4 @1  46546xW%0l 1 5',
      'Origin',
        'http://example.com',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'multiple connection header values with folding and lws',
    type: REQUEST,
    raw: [
      'GET /demo HTTP/1.1',
      'Connection: keep-alive, upgrade',
      'Upgrade: WebSocket',
      '', '',
      'Hot diggity dogg'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/demo',
    headers: [
      'Connection',
        'keep-alive, upgrade',
      'Upgrade',
        'WebSocket',
    ],
    upgrade: true,
    body: undefined
  },
  {
    name: 'multiple connection header values with folding and lws and CRLF',
    type: REQUEST,
    raw: [
      'GET /demo HTTP/1.1',
      'Connection: keep-alive, ',
      ' upgrade',
      'Upgrade: WebSocket',
      '', '',
      'Hot diggity dogg'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    method: 'GET',
    url: '/demo',
    headers: [
      'Connection',
        'keep-alive,  upgrade',
      'Upgrade',
        'WebSocket',
    ],
    upgrade: true,
    body: undefined
  },
  // RESPONSES =================================================================
  {
    name: 'google 301',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 301 Moved Permanently',
      'Location: http://www.google.com/',
      'Content-Type: text/html; charset=UTF-8',
      'Date: Sun, 26 Apr 2009 11:11:49 GMT',
      'Expires: Tue, 26 May 2009 11:11:49 GMT',
      'X-$PrototypeBI-Version: 1.6.0.3',
      'Cache-Control: public, max-age=2592000',
      'Server: gws',
      'Content-Length:  219  ',
      '',
      '<HTML><HEAD><meta http-equiv="content-type" content="text/html;'
        + 'charset=utf-8">\n<TITLE>301 Moved</TITLE></HEAD><BODY>\n<H1>301 '
        + 'Moved</H1>\nThe document has moved\n<A HREF="http://www.google.com/'
        + '">here</A>.\r\n</BODY></HTML>\r\n'
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 301,
    statusText: 'Moved Permanently',
    headers: [
      'Location',
        'http://www.google.com/',
      'Content-Type',
        'text/html; charset=UTF-8',
      'Date',
        'Sun, 26 Apr 2009 11:11:49 GMT',
      'Expires',
        'Tue, 26 May 2009 11:11:49 GMT',
      'X-$PrototypeBI-Version',
        '1.6.0.3',
      'Cache-Control',
        'public, max-age=2592000',
      'Server',
        'gws',
      'Content-Length',
        '219',
    ],
    upgrade: false,
    body: '<HTML><HEAD><meta http-equiv="content-type" content="text/html;'
        + 'charset=utf-8">\n<TITLE>301 Moved</TITLE></HEAD><BODY>\n<H1>301 '
        + 'Moved</H1>\nThe document has moved\n<A HREF="http://www.google.com/'
        + '">here</A>.\r\n</BODY></HTML>\r\n'
  },
  {
    name: 'no content-length response',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Date: Tue, 04 Aug 2009 07:59:32 GMT',
      'Server: Apache',
      'X-Powered-By: Servlet/2.5 JSP/2.1',
      'Content-Type: text/xml; charset=utf-8',
      'Connection: close',
      '',
      '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SOAP-ENV:Envelope xmlns'
        + ':SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n  '
        + '<SOAP-ENV:Body>\n    <SOAP-ENV:Fault>\n       <faultcode>SOAP-ENV:'
        + 'Client</faultcode>\n       <faultstring>Client Error</faultstring>\n'
        + '    </SOAP-ENV:Fault>\n  </SOAP-ENV:Body>\n</SOAP-ENV:Envelope>'
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Date',
        'Tue, 04 Aug 2009 07:59:32 GMT',
      'Server',
        'Apache',
      'X-Powered-By',
        'Servlet/2.5 JSP/2.1',
      'Content-Type',
        'text/xml; charset=utf-8',
      'Connection',
        'close',
    ],
    upgrade: false,
    body: '<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<SOAP-ENV:Envelope xmlns'
        + ':SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\">\n  '
        + '<SOAP-ENV:Body>\n    <SOAP-ENV:Fault>\n       <faultcode>SOAP-ENV:'
        + 'Client</faultcode>\n       <faultstring>Client Error</faultstring>\n'
        + '    </SOAP-ENV:Fault>\n  </SOAP-ENV:Body>\n</SOAP-ENV:Envelope>'
  },
  {
    name: '404 no headers no body',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 404 Not Found',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 404,
    statusText: 'Not Found',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: '301 no response phrase',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 301',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 301,
    statusText: '',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: '200 trailing space on chunked body',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Content-Type: text/plain',
      'Transfer-Encoding: chunked',
      '',
      '25  ',
      'This is the data in the first chunk\r\n',
      '1C',
      'and this is the second one\r\n',
      '0  ',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Content-Type',
        'text/plain',
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: 'This is the data in the first chunk\r\n'
        + 'and this is the second one\r\n'
  },
  {
    name: 'underscore header key',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Server: DCLK-AdSvr',
      'Content-Type: text/xml',
      'Content-Length: 0',
      'DCLK_imp: v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;'
        + 'dcmt=text/xml;;~cs=o',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Server',
        'DCLK-AdSvr',
      'Content-Type',
        'text/xml',
      'Content-Length',
        '0',
      'DCLK_imp',
        'v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;'
          + 'dcmt=text/xml;;~cs=o',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'no merge with empty value',
    type: RESPONSE,
    raw: [
      'HTTP/1.0 301 Moved Permanently',
      'Date: Thu, 03 Jun 2010 09:56:32 GMT',
      'Server: Apache/2.2.3 (Red Hat)',
      'Cache-Control: public',
      'Pragma: ',
      'Location: http://www.example.org/',
      'Vary: Accept-Encoding',
      'Content-Length: 0',
      'Content-Type: text/html; charset=UTF-8',
      'Connection: keep-alive',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    statusCode: 301,
    statusText: 'Moved Permanently',
    headers: [
      'Date',
        'Thu, 03 Jun 2010 09:56:32 GMT',
      'Server',
        'Apache/2.2.3 (Red Hat)',
      'Cache-Control',
        'public',
      'Pragma',
        '',
      'Location',
        'http://www.example.org/',
      'Vary',
        'Accept-Encoding',
      'Content-Length',
        '0',
      'Content-Type',
        'text/html; charset=UTF-8',
      'Connection',
        'keep-alive',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'field underscore',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Date: Tue, 28 Sep 2010 01:14:13 GMT',
      'Server: Apache',
      'Cache-Control: no-cache, must-revalidate',
      'Expires: Mon, 26 Jul 1997 05:00:00 GMT',
      '.et-Cookie: ExampleCS=1274804622353690521; path=/; domain=.example.com',
      'Vary: Accept-Encoding',
      '_eep-Alive: timeout=45',
      '_onnection: Keep-Alive',
      'Transfer-Encoding: chunked',
      'Content-Type: text/html',
      'Connection: close',
      '',
      '0',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Date',
        'Tue, 28 Sep 2010 01:14:13 GMT',
      'Server',
        'Apache',
      'Cache-Control',
        'no-cache, must-revalidate',
      'Expires',
        'Mon, 26 Jul 1997 05:00:00 GMT',
      '.et-Cookie',
        'ExampleCS=1274804622353690521; path=/; domain=.example.com',
      'Vary',
        'Accept-Encoding',
      '_eep-Alive',
        'timeout=45',
      '_onnection',
        'Keep-Alive',
      'Transfer-Encoding',
        'chunked',
      'Content-Type',
        'text/html',
      'Connection',
        'close',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'non-ASCII in status line',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 500 Oriëntatieprobleem',
      'Date: Fri, 5 Nov 2010 23:07:12 GMT+2',
      'Content-Length: 0',
      'Connection: close',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 500,
    statusText: 'Oriëntatieprobleem',
    headers: [
      'Date',
        'Fri, 5 Nov 2010 23:07:12 GMT+2',
      'Content-Length',
        '0',
      'Connection',
        'close',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'neither content-length nor transfer-encoding response',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Content-Type: text/plain',
      '',
      'hello world'
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Content-Type',
        'text/plain',
    ],
    upgrade: false,
    body: 'hello world'
  },
  {
    name: 'HTTP/1.0 with keep-alive and EOF-terminated 200 status',
    type: RESPONSE,
    raw: [
      'HTTP/1.0 200 OK',
      'Connection: keep-alive',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 0,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Connection',
        'keep-alive',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'HTTP/1.0 with keep-alive and a 204 status',
    type: RESPONSE,
    raw: [
      'HTTP/1.0 204 No content',
      'Connection: keep-alive',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 0,
    statusCode: 204,
    statusText: 'No content',
    headers: [
      'Connection',
        'keep-alive',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'HTTP/1.1 with an EOF-terminated 200 status',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'HTTP/1.1 with a 204 status',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 204 No content',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 204,
    statusText: 'No content',
    headers: [],
    upgrade: false,
    body: undefined
  },
  {
    name: 'HTTP/1.1 with a 204 status and keep-alive disabled',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 204 No content',
      'Connection: close',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 204,
    statusText: 'No content',
    headers: [
      'Connection',
        'close',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'HTTP/1.1 with chunked endocing and a 200 response',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 OK',
      'Transfer-Encoding: chunked',
      '',
      '0',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: 'OK',
    headers: [
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: undefined
  },
  {
    name: 'newline chunk',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 301 MovedPermanently',
      'Date: Wed, 15 May 2013 17:06:33 GMT',
      'Server: Server',
      'x-amz-id-1: 0GPHKXSJQ826RK7GZEB2',
      'p3p: policyref="http://www.amazon.com/w3c/p3p.xml",CP="CAO DSP LAW CUR '
        + 'ADM IVAo IVDo CONo OTPo OUR DELi PUBi OTRi BUS PHY ONL UNI PUR FIN '
        + 'COM NAV INT DEM CNT STA HEA PRE LOC GOV OTC "',
      'x-amz-id-2: STN69VZxIFSz9YJLbz1GDbxpbjG6Qjmmq5E3DxRhOUw+Et0p4hr7c/Q8qNc'
        + 'x4oAD',
      'Location: http://www.amazon.com/Dan-Brown/e/B000AP9DSU/ref=s9_pop_gw_al'
        + '1?_encoding=UTF8&refinementId=618073011&pf_rd_m=ATVPDKIKX0DER&pf_rd'
        + '_s=center-2&pf_rd_r=0SHYY5BZXN3KR20BNFAY&pf_rd_t=101&pf_rd_p=126334'
        + '0922&pf_rd_i=507846',
      'Vary: Accept-Encoding,User-Agent',
      'Content-Type: text/html; charset=ISO-8859-1',
      'Transfer-Encoding: chunked',
      '',
      '1',
      '\n',
      '0',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: true,
    msgCompleteOnEOF: false,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 301,
    statusText: 'MovedPermanently',
    headers: [
      'Date',
        'Wed, 15 May 2013 17:06:33 GMT',
      'Server',
        'Server',
      'x-amz-id-1',
        '0GPHKXSJQ826RK7GZEB2',
      'p3p',
        'policyref="http://www.amazon.com/w3c/p3p.xml",CP="CAO DSP LAW CUR '
        + 'ADM IVAo IVDo CONo OTPo OUR DELi PUBi OTRi BUS PHY ONL UNI PUR FIN '
        + 'COM NAV INT DEM CNT STA HEA PRE LOC GOV OTC "',
      'x-amz-id-2',
        'STN69VZxIFSz9YJLbz1GDbxpbjG6Qjmmq5E3DxRhOUw+Et0p4hr7c/Q8qNc'
        + 'x4oAD',
      'Location',
        'http://www.amazon.com/Dan-Brown/e/B000AP9DSU/ref=s9_pop_gw_al'
        + '1?_encoding=UTF8&refinementId=618073011&pf_rd_m=ATVPDKIKX0DER&pf_rd'
        + '_s=center-2&pf_rd_r=0SHYY5BZXN3KR20BNFAY&pf_rd_t=101&pf_rd_p=126334'
        + '0922&pf_rd_i=507846',
      'Vary',
        'Accept-Encoding,User-Agent',
      'Content-Type',
        'text/html; charset=ISO-8859-1',
      'Transfer-Encoding',
        'chunked',
    ],
    upgrade: false,
    body: '\n'
  },
  {
    name: 'empty reason phrase after space',
    type: RESPONSE,
    raw: [
      'HTTP/1.1 200 ',
      '', ''
    ].join(CRLF),
    shouldKeepAlive: false,
    msgCompleteOnEOF: true,
    httpMajor: 1,
    httpMinor: 1,
    statusCode: 200,
    statusText: '',
    headers: [],
    upgrade: false,
    body: undefined
  },
];


// Prevent EE warnings since we have many test cases which attach `exit` event
// handlers
process.setMaxListeners(0);

cases.forEach(function(testCase) {
  var parser = new HTTPParser(testCase.type);
  var reqEvents = [ 'onHeaders' ];
  var completed = false;
  var allHeaders;
  var body;

  if (testCase.body !== undefined)
    reqEvents.push('onBody');

  function onHeaders(versionMajor, versionMinor, headers, method, url,
                     statusCode, statusText, upgrade, shouldKeepAlive) {
    assert.strictEqual(reqEvents[0],
                       'onHeaders',
                       'Expected onHeaders to the next event for: '
                         + testCase.name);
    reqEvents.shift();
    _assert(assert.strictEqual,
            'versionMajor',
            testCase,
            versionMajor,
            testCase.httpMajor);
    _assert(assert.strictEqual,
            'versionMinor',
            testCase,
            versionMinor,
            testCase.httpMinor);
    // Defer checking headers in case there are trailers ...
    allHeaders = headers;
    if (testCase.type === REQUEST) {
      _assert(assert.strictEqual,
              'method',
              testCase,
              method,
              testCase.method);
      _assert(assert.strictEqual,
              'url',
              testCase,
              url,
              testCase.url);
    } else {
      _assert(assert.strictEqual,
              'statusCode',
              testCase,
              statusCode,
              testCase.statusCode);
      _assert(assert.strictEqual,
              'statusText',
              testCase,
              statusText,
              testCase.statusText);
    }
    _assert(assert.strictEqual,
            'upgrade',
            testCase,
            upgrade,
            testCase.upgrade);
    _assert(assert.strictEqual,
            'shouldKeepAlive',
            testCase,
            shouldKeepAlive,
            testCase.shouldKeepAlive);
  }

  function onBody(data, offset, len) {
    if (body === undefined) {
      assert.strictEqual(reqEvents[0],
                         'onBody',
                         'Expected onBody to be the next event for: '
                           + testCase.name);
      reqEvents.shift();
      body = data.toString('binary', offset, offset + len);
    } else
      body += data.toString('binary', offset, offset + len);
  }

  function onComplete() {
    assert.strictEqual(reqEvents.length,
                       0,
                       'Missed ' + reqEvents + ' event(s) for: ' +
                         testCase.name);
    if (parser.headers.length > 0)
      allHeaders = allHeaders.concat(parser.headers);
    _assert(assert.deepEqual,
            'headers',
            testCase,
            allHeaders,
            testCase.headers);
    _assert(assert.strictEqual,
            'body',
            testCase,
            body,
            testCase.body);
    completed = true;
  }

  parser.onHeaders = onHeaders;
  parser.onBody = onBody;
  parser.onComplete = onComplete;

  process.on('exit', function() {
    assert.strictEqual(completed,
                       true,
                       'Parsing did not complete for: ' + testCase.name);
  });

  try {
    var ret = parser.execute(new Buffer(testCase.raw, 'binary'));
    parser.finish();
  } catch (ex) {
    throw new Error('Unexpected error thrown for: ' + testCase.name + ':\n\n' +
                    ex.stack + '\n');
  }
  if (testCase.error !== undefined && typeof ret === 'number')
    throw new Error('Expected error for: ' + testCase.name);
  else if (testCase.error === undefined && typeof ret !== 'number') {
    throw new Error('Unexpected error for: ' + testCase.name + ':\n\n' +
                    ret.stack + '\n');
  }
});

function _assert(assertFn, type, testCase, actual, expected) {
  assertFn(actual,
           expected,
           type + ' mismatch for: ' + testCase.name + '\nActual:\n' +
             inspect(actual) + '\nExpected:\n' + inspect(expected) + '\n');
}
