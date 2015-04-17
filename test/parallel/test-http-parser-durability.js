var assert = require('assert');
var inspect = require('util').inspect;
var format = require('util').format;

var HTTPParser = require('_http_parser');

var CRLF = '\r\n';
var REQUEST = HTTPParser.REQUEST;
var RESPONSE = HTTPParser.RESPONSE;
var requestsEnd = -1;

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
      '',
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
    upgrade: 'Hot diggity dogg',
    body: undefined
  },
  {
    name: 'connect request',
    type: REQUEST,
    raw: [
      'CONNECT 0-home0.netscape.com:443 HTTP/1.0',
      'User-agent: Mozilla/1.1N',
      'Proxy-authorization: basic aGVsbG86d29ybGQ=',
      '',
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
    upgrade: 'some data\r\nand yet even more data',
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
    upgrade: '',
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
    upgrade: '',
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
      '',
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
    upgrade: 'Hot diggity dogg',
    body: undefined
  },
  {
    name: 'multiple connection header values with folding and lws',
    type: REQUEST,
    raw: [
      'GET /demo HTTP/1.1',
      'Connection: keep-alive, upgrade',
      'Upgrade: WebSocket',
      '',
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
    upgrade: 'Hot diggity dogg',
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
      '',
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
    upgrade: 'Hot diggity dogg',
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
    body: undefined
  },
  {
    name: 'HTTP/1.1 with chunked encoding and a 200 response',
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
    body: undefined
  },
];
for (var i = 0; i < cases.length; ++i) {
  if (cases[i].type === RESPONSE) {
    requestsEnd = i - 1;
    break;
  }
}



// Prevent EE warnings since we have many test cases which attach `exit` event
// handlers
process.setMaxListeners(0);

// Test predefined requests/responses
cases.forEach(function(testCase) {
  var parser = new HTTPParser(testCase.type);
  var input = new Buffer(testCase.raw, 'binary');
  var reqEvents = ['onHeaders'];
  var completed = false;
  var message = {};

  if (testCase.body !== undefined)
    reqEvents.push('onBody');

  function onHeaders(versionMajor, versionMinor, headers, method, url,
                     statusCode, statusText, upgrade, shouldKeepAlive) {
    assert.strictEqual(reqEvents[0],
                       'onHeaders',
                       'Expected onHeaders to the next event for: ' +
                         testCase.name);
    reqEvents.shift();
    message = {
      type: (method === undefined && url === undefined ? RESPONSE : REQUEST),
      shouldKeepAlive: shouldKeepAlive,
      //msgCompleteOnEOF
      httpMajor: versionMajor,
      httpMinor: versionMinor,
      method: method,
      url: url,
      headers: headers,
      statusCode: statusCode,
      statusText: statusText,
      upgrade: upgrade
    };
  }

  function onBody(data, offset, len) {
    if (message.body === undefined) {
      assert.strictEqual(reqEvents[0],
                         'onBody',
                         'Expected onBody to be the next event for: ' +
                           testCase.name);
      reqEvents.shift();
      message.body = data.toString('binary', offset, offset + len);
    } else
      message.body += data.toString('binary', offset, offset + len);
  }

  function onComplete() {
    assert.strictEqual(reqEvents.length,
                       0,
                       'Missed ' + reqEvents + ' event(s) for: ' +
                         testCase.name);
    if (parser.headers.length > 0) {
      if (message.headers)
        message.headers = message.headers.concat(parser.headers);
      else
        message.headers = parser.headers;
    }
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

  var ret;
  try {
    ret = parser.execute(input);
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
  if (message.upgrade === false || typeof ret !== 'number')
    message.upgrade = undefined;
  else
    message.upgrade = input.toString('binary', ret);
  assertMessageEquals(message, testCase);
});

// Test execute() return value
(function() {
  var parser = new HTTPParser(REQUEST);
  var input = 'GET / HTTP/1.1\r\nheader: value\r\nhdr: value\r\n';
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(ret, Buffer.byteLength(input));
})();

// Test for header overflow
[REQUEST, RESPONSE].forEach(function(type) {
  var parser = new HTTPParser(type);
  var input = (type === REQUEST ? 'GET / HTTP/1.1\r\n' : 'HTTP/1.0 200 OK\r\n');
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(ret, Buffer.byteLength(input));

  input = new Buffer('header-key: header-value\r\n');
  for (var i = 0; i < 10000; ++i) {
    ret = parser.execute(input);
    if (typeof ret !== 'number') {
      assert(/Header size limit exceeded/i.test(ret.message));
      return;
    }
  }

  throw new Error('Error expected but none in header overflow test');
});

// Test for no overflow with long body
[REQUEST, RESPONSE].forEach(function(type) {
  [1000, 100000].forEach(function(length) {
    var parser = new HTTPParser(type);
    var input = format(
      '%s\r\nConnection: Keep-Alive\r\nContent-Length: %d\r\n\r\n',
      type === REQUEST ? 'POST / HTTP/1.0' : 'HTTP/1.0 200 OK',
      length
    );
    var input2 = new Buffer('a');
    var ret;

    parser.onHeaders = parser.onBody = parser.onComplete = function() {};
    ret = parser.execute(new Buffer(input));
    assert.strictEqual(ret, Buffer.byteLength(input));

    for (var i = 0; i < length; ++i) {
      ret = parser.execute(input2);
      assert.strictEqual(ret, 1);
    }

    ret = parser.execute(new Buffer(input));
    assert.strictEqual(ret, Buffer.byteLength(input));
  });
});

// Test for content length overflow
['9007199254740991', '9007199254740992', '9007199254740993'].forEach(
  function(length, i) {
    var parser = new HTTPParser(RESPONSE);
    var input = format('HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\n', length);
    var ret;

    parser.onHeaders = parser.onBody = parser.onComplete = function() {};
    ret = parser.execute(new Buffer(input));
    if (i === 0)
      assert.strictEqual(ret, Buffer.byteLength(input));
    else {
      assert.strictEqual(typeof ret !== 'number', true);
      assert.strictEqual(/Bad Content-Length/i.test(ret.message), true);
    }
  }
);

// Test for chunk length overflow
['1fffffffffffff', '20000000000000', '20000000000001'].forEach(
  function(length, i) {
    var parser = new HTTPParser(RESPONSE);
    var input = format('HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n' +
                       '%s\r\n...', length);
    var ret;

    parser.onHeaders = parser.onBody = parser.onComplete = function() {};
    ret = parser.execute(new Buffer(input));
    if (i === 0)
      assert.strictEqual(ret, Buffer.byteLength(input));
    else {
      assert.strictEqual(typeof ret !== 'number', true);
      assert.strictEqual(/Chunk size too big/i.test(ret.message), true);
    }
  }
);

// Test pipelined responses
(function() {
  var responsesStart = requestsEnd + 1;
  for (var i = responsesStart; i < cases.length; ++i) {
    if (!cases[i].shouldKeepAlive)
      continue;
    for (var j = responsesStart; j < cases.length; ++j) {
      if (!cases[j].shouldKeepAlive)
        continue;
      for (var k = responsesStart; k < cases.length; ++k)
        testMultiple3(cases[i], cases[j], cases[k]);
    }
  }
})();

// Test response body sizes
[
 getMessageByName('404 no headers no body'),
 getMessageByName('200 trailing space on chunked body'),
 {
   name: 'large chunked message',
   type: RESPONSE,
   raw: createLargeChunkedMessage(31337, [
     'HTTP/1.0 200 OK',
     'Transfer-Encoding: chunked',
     'Content-Type: text/plain',
     '', ''
   ].join(CRLF)),
   shouldKeepAlive: false,
   msgCompleteOnEOF: false,
   httpMajor: 1,
   httpMinor: 0,
   statusCode: 200,
   statusText: 'OK',
   headers: [
     'Transfer-Encoding',
       'chunked',
     'Content-Type',
       'text/plain'
   ],
   bodySize: 31337 * 1024
 }
].forEach(function(expected) {
  var parser = new HTTPParser(expected.type);
  var expectedBodySize = (expected.bodySize !== undefined
                          ? expected.bodySize
                          : (expected.body && expected.body.length) || 0);
  var messages = [];
  var message = {};
  var ret;
  var body;

  parser.onHeaders = function(versionMajor, versionMinor, headers, method, url,
                              statusCode, statusText, upgrade,
                              shouldKeepAlive) {
    message = {
      type: (method === undefined && url === undefined ? RESPONSE : REQUEST),
      shouldKeepAlive: shouldKeepAlive,
      //msgCompleteOnEOF
      httpMajor: versionMajor,
      httpMinor: versionMinor,
      method: method,
      url: url,
      headers: headers,
      statusCode: statusCode,
      statusText: statusText
    };
  };
  parser.onBody = function(data, offset, len) {
    if (message.bodySize === undefined) {
      message.bodySize = len;
      body = data.toString('binary', offset, offset + len);
    } else {
      message.bodySize += len;
      body += data.toString('binary', offset, offset + len);
    }
  };
  parser.onComplete = function() {
    messages.push(message);
    message = {};
  };

  var l = expected.raw.length;
  var chunk = 4024;

  for (var i = 0; i < l; i += chunk) {
    var toread = Math.min(l - i, chunk);
    ret = parser.execute(
      new Buffer(expected.raw.slice(i, i + toread), 'binary')
    );
    assert.strictEqual(ret, toread);
  }
  assert.strictEqual(parser.finish(), undefined);

  assert.strictEqual(messages.length, 1);
  assertMessageEquals(messages[0], expected, ['body', 'upgrade']);
  assert.strictEqual(messages[0].bodySize || 0, expectedBodySize);
});


// Perform scan tests on some responses
console.log('response scan 1/2      ');
testScan(getMessageByName('200 trailing space on chunked body'),
         getMessageByName('HTTP/1.0 with keep-alive and a 204 status'),
         getMessageByName('301 no response phrase'));
console.log('response scan 2/2      ');
testScan(getMessageByName('no merge with empty value'),
         getMessageByName('underscore header key'),
         {
           name: 'ycombinator headers',
           type: RESPONSE,
           raw: [
             'HTTP/1.1 200 OK',
             'Content-Type: text/html; charset=utf-8',
             'Connection: close',
             '',
             'these headers are from http://news.ycombinator.com/'
           ].join(CRLF),
           shouldKeepAlive: false,
           msgCompleteOnEOF: true,
           httpMajor: 1,
           httpMinor: 1,
           statusCode: 200,
           statusText: 'OK',
           headers: [
             'Content-Type',
               'text/html; charset=utf-8',
             'Connection',
               'close',
           ],
           body: 'these headers are from http://news.ycombinator.com/'
         });
console.log('responses okay');




// Test malformed HTTP version in request
(function() {
  var parser = new HTTPParser(REQUEST);
  var input = 'GET / HTP/1.1\r\n\r\n';
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(typeof ret !== 'number', true);
  assert.strictEqual(/Malformed request line/i.test(ret.message), true);
})();

// Test well-formed but incomplete request
(function() {
  var parser = new HTTPParser(REQUEST);
  var input = 'GET / HTTP/1.1\r\nContent-Type: text/plain\r\n' +
              'Content-Length: 6\r\n\r\nfooba';
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(ret, input.length);
})();

// Test illegal header field name line folding in request
(function() {
  var parser = new HTTPParser(REQUEST);
  var input = 'GET / HTTP/1.1\r\nname\r\n : value\r\n\r\n';
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(typeof ret !== 'number', true);
  assert.strictEqual(/Malformed header line/i.test(ret.message), true);
})();

// Test large SSL certificate header value in request
(function() {
  var parser = new HTTPParser(REQUEST);
  var input =
    'GET / HTTP/1.1\r\n' +
    'X-SSL-Bullshit:   -----BEGIN CERTIFICATE-----\r\n' +
    '\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx\r\n' +
    '\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT\r\n' +
    '\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu\r\n' +
    '\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV\r\n' +
    '\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV\r\n' +
    '\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB\r\n' +
    '\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF\r\n' +
    '\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR\r\n' +
    '\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL\r\n' +
    '\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP\r\n' +
    '\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR\r\n' +
    '\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG\r\n' +
    '\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgHTTPAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs\r\n' +
    '\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD\r\n' +
    '\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj\r\n' +
    '\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj\r\n' +
    '\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG\r\n' +
    '\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE\r\n' +
    '\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO\r\n' +
    '\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1\r\n' +
    '\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBD' +
    'AWLmh0\r\n' +
    '\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD\r\n' +
    '\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv\r\n' +
    '\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3\r\n' +
    '\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8\r\n' +
    '\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk\r\n' +
    '\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK\r\n' +
    '\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu\r\n' +
    '\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3\r\n' +
    '\tRA==\r\n' +
    '\t-----END CERTIFICATE-----\r\n' +
    '\r\n';
  var ret;

  parser.onHeaders = parser.onBody = parser.onComplete = function() {};
  ret = parser.execute(new Buffer(input));
  assert.strictEqual(ret, input.length);
})();


// Test pipelined requests
(function() {
  for (var i = 0; i <= requestsEnd; ++i) {
    if (!cases[i].shouldKeepAlive)
      continue;
    for (var j = 0; j <= requestsEnd; ++j) {
      if (!cases[j].shouldKeepAlive)
        continue;
      for (var k = 0; k <= requestsEnd; ++k)
        testMultiple3(cases[i], cases[j], cases[k]);
    }
  }
})();


// Perform scan tests on some requests
console.log('request scan 1/4      ');
testScan(getMessageByName('get no headers no body'),
         getMessageByName('get one header no body'),
         getMessageByName('get no headers no body'));
console.log('request scan 2/4      ');
testScan(getMessageByName(
          'post - chunked body: all your base are belong to us'
         ),
         getMessageByName('post identity body world'),
         getMessageByName('get funky content length body hello'));
console.log('request scan 3/4      ');
testScan(getMessageByName('two chunks ; triple zero ending'),
         getMessageByName('chunked with trailing headers'),
         getMessageByName('chunked with chunk extensions'));
console.log('request scan 4/4      ');
testScan(getMessageByName('query url with question mark'),
         getMessageByName('newline prefix get'),
         getMessageByName('connect request'));
console.log('requests okay');




// HELPER FUNCTIONS ============================================================

// SCAN through every possible breaking to make sure the parser can handle
// getting the content in any chunks that might come from the socket
function testScan(case1, case2, case3) {
  var messageCount = countParsedMessages(case1, case2, case3);
  var total = case1.raw + case2.raw + case3.raw;
  var totallen = total.length;
  var totalops = (totallen - 1) * (totallen - 2) / 2;
  var messages = [];
  var message = {};
  var ops = 0;
  var nb = 0;
  var hasUpgrade;
  var ret;

  function onHeaders(versionMajor, versionMinor, headers, method, url,
                     statusCode, statusText, upgrade, shouldKeepAlive) {
    message = {
      type: (method === undefined && url === undefined ? RESPONSE : REQUEST),
      shouldKeepAlive: shouldKeepAlive,
      //msgCompleteOnEOF
      httpMajor: versionMajor,
      httpMinor: versionMinor,
      method: method,
      url: url,
      headers: headers,
      statusCode: statusCode,
      statusText: statusText,
      upgrade: upgrade
    };
  }
  function onBody(data, offset, len) {
    if (!message.body)
      message.body = data.toString('binary', offset, offset + len);
    else
      message.body += data.toString('binary', offset, offset + len);
  }
  function onComplete() {
    if (parser.headers.length > 0) {
      if (message.headers)
        message.headers = message.headers.concat(parser.headers);
      else
        message.headers = parser.headers;
    }
    messages.push(message);
    message = {};
  }

  for (var j = 2; j < totallen; ++j) {
    for (var i = 1; i < j; ++i) {
      if (ops % 1000 === 0) {
        var value = Math.floor(100 * ops / totalops);
        if (value < 10)
          value = '  ' + value;
        else if (value < 100)
          value = ' ' + value;
        else
          value = '' + value;
        console.log('\b\b\b\b%s%', value);
      }
      ++ops;

      var parser = new HTTPParser(case1.type);
      parser.onHeaders = onHeaders;
      parser.onBody = onBody;
      parser.onComplete = onComplete;

      messages = [];
      hasUpgrade = false;
      nb = 0;

      ret = parser.execute(new Buffer(total.slice(0, i), 'binary'));
      assert.strictEqual(typeof ret === 'number', true);
      nb += ret;

      for (var k = 0; k < messages.length; ++k) {
        if (messages[k].upgrade === true)
          hasUpgrade = true;
        else
          delete messages[k].upgrade;
      }

      if (!hasUpgrade) {
        assert.strictEqual(nb, i);

        ret = parser.execute(new Buffer(total.slice(i, j), 'binary'));
        assert.strictEqual(typeof ret === 'number', true);
        nb += ret;

        for (var k = 0; k < messages.length; ++k) {
          if (messages[k].upgrade === true)
            hasUpgrade = true;
          else
            delete messages[k].upgrade;
        }

        if (!hasUpgrade) {
          assert.strictEqual(nb, i + (j - i));

          ret = parser.execute(new Buffer(total.slice(j), 'binary'));
          assert.strictEqual(typeof ret === 'number', true);
          nb += ret;

          for (var k = 0; k < messages.length; ++k) {
            if (messages[k].upgrade === true)
              hasUpgrade = true;
            else
              delete messages[k].upgrade;
          }

          if (!hasUpgrade)
            assert.strictEqual(nb, i + (j - i) + (totallen - j));
        }
      }

      assert.strictEqual(parser.finish(), undefined);
      assert.strictEqual(messages.length, messageCount);

      for (var k = 0; k < messages.length; ++k) {
        if (messages[k].upgrade !== true)
          delete messages[k].upgrade;
      }

      if (hasUpgrade) {
        var lastMessage = messages.slice(-1)[0];
        upgradeMessageFix(total, nb, lastMessage, case1, case2, case3);
      }

      assertMessageEquals(messages[0], case1);
      if (messages.length > 1)
        assertMessageEquals(messages[1], case2);
      if (messages.length > 2)
        assertMessageEquals(messages[2], case3);
    }
  }
  console.log('\b\b\b\b100%');
}

function testMultiple3(case1, case2, case3) {
  var messageCount = countParsedMessages(case1, case2, case3);
  var total = case1.raw + case2.raw + case3.raw;
  var parser = new HTTPParser(case1.type);
  var messages = [];
  var message = {};
  var ret;

  parser.onHeaders = function(versionMajor, versionMinor, headers, method, url,
                              statusCode, statusText, upgrade,
                              shouldKeepAlive) {
    message = {
      type: (method === undefined && url === undefined ? RESPONSE : REQUEST),
      shouldKeepAlive: shouldKeepAlive,
      //msgCompleteOnEOF
      httpMajor: versionMajor,
      httpMinor: versionMinor,
      method: method,
      url: url,
      headers: headers,
      statusCode: statusCode,
      statusText: statusText,
      upgrade: upgrade
    };
  };
  parser.onBody = function(data, offset, len) {
    if (!message.body)
      message.body = data.toString('binary', offset, offset + len);
    else
      message.body += data.toString('binary', offset, offset + len);
  };
  parser.onComplete = function() {
    if (parser.headers.length > 0) {
      if (message.headers)
        message.headers = message.headers.concat(parser.headers);
      else
        message.headers = parser.headers;
    }
    messages.push(message);
    message = {};
  };

  ret = parser.execute(new Buffer(total, 'binary'));

  assert.strictEqual(parser.finish(), undefined);
  assert.strictEqual(messages.length, messageCount);

  var hasUpgrade = false;
  for (var i = 0; i < messages.length; ++i) {
    if (messages[i].upgrade === true)
      hasUpgrade = true;
    else
      delete messages[i].upgrade;
  }

  if (hasUpgrade) {
    var lastMessage = messages.slice(-1)[0];
    upgradeMessageFix(total, ret, lastMessage, case1, case2, case3);
  } else
    assert.strictEqual(ret, total.length);

  assertMessageEquals(messages[0], case1);
  if (messages.length > 1)
    assertMessageEquals(messages[1], case2);
  if (messages.length > 2)
    assertMessageEquals(messages[2], case3);
}

function upgradeMessageFix(body, ret, actualLast) {
  var offset = 0;

  for (var i = 3; i < arguments.length; ++i) {
    var caseMsg = arguments[i];

    offset += caseMsg.raw.length;

    if (caseMsg.upgrade !== undefined) {
      offset -= caseMsg.upgrade.length;

      // Check the portion of the response after its specified upgrade
      assert.strictEqual(body.slice(offset), body.slice(ret));

      // Fix up the response so that assertMessageEquals() will verify the
      // upgrade correctly
      actualLast.upgrade = body.slice(ret, ret + caseMsg.upgrade.length);
      return;
    }
  }

  throw new Error('Expected a message with an upgrade');
}

function countParsedMessages() {
  for (var i = 0; i < arguments.length; ++i) {
    if (arguments[i].upgrade) {
      return i + 1;
    }
  }
  return arguments.length;
}

function createLargeChunkedMessage(bodySizeKB, rawHeaders) {
  var wrote = 0;
  var headerslen = rawHeaders.length;
  var bufsize = headerslen + (5 + 1024 + 2) * bodySizeKB + 5;
  var buf = new Buffer(bufsize);

  buf.write(rawHeaders, wrote, headerslen, 'binary');
  wrote += headerslen;

  for (var i = 0; i < bodySizeKB; ++i) {
    // Write 1KB chunk into the body.
    buf.write('400\r\n', wrote, 5);
    wrote += 5;
    buf.fill('C', wrote, wrote + 1024);
    wrote += 1024;
    buf.write('\r\n', wrote, 2);
    wrote += 2;
  }

  buf.write('0\r\n\r\n', wrote, 5);
  wrote += 5;
  assert.strictEqual(wrote, bufsize);

  return buf.toString('binary');
}

function getMessageByName(name) {
  var lowered = name.toLowerCase();
  for (var i = 0; i < cases.length; ++i) {
    if (cases[i].name.toLowerCase() === lowered)
      return cases[i];
  }
  throw new Error('Predefined HTTP message not found for: ' + name);
}

function assertMessageEquals(actual, expected, except) {
  ['type', 'httpMajor', 'httpMinor', 'method', 'url', 'statusCode',
   'statusText', 'shouldKeepAlive', 'headers', 'upgrade', 'body'
  ].filter(function(p) {
    return (except === undefined || except.indexOf(p) === -1);
  }).forEach(function(type) {
    var assertFn = (type === 'headers' ? assert.deepEqual : assert.strictEqual);
    assertFn(actual[type],
             expected[type],
             type + ' mismatch for: ' + expected.name + '\nActual:\n' +
               inspect(actual[type]) + '\nExpected:\n' +
               inspect(expected[type]) + '\n');
  });
}
