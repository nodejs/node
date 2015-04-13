var common = require('../common.js');
var HTTPParser = require('_http_parser');
var CRLF = '\r\n';
var REQUEST = HTTPParser.REQUEST;
var RESPONSE = HTTPParser.RESPONSE;

var bench = common.createBenchmark(main, {
  type: [
    'small-req',
    'small-res',
    'small-req-chunked',
    'small-res-chunked',
    'medium-req',
    'medium-res',
    'medium-req-chunked',
    'medium-res-chunked',
    'large-req',
    'large-res',
    'large-req-chunked',
    'large-res-chunked',
  ]
});

var inputs = {
  'small-req': [
    'GET /index.html HTTP/1.1' + CRLF +
    'Host: www.example.com' + CRLF + CRLF
  ],
  'small-res': [
    'HTTP/1.1 200 OK' + CRLF +
    'Date: Mon, 23 May 2005 22:38:34 GMT' + CRLF + CRLF
  ],
  'small-req-chunked': [
    'GET',
    ' /index',
    '.html HT',
    'TP/1.',
    '1',
    CRLF,
    'Host',
    ': ',
    'www.example.com' + CRLF,
    CRLF
  ],
  'small-res-chunked': [
    'HTTP',
    '/1.',
    '1 20',
    '0 OK' + CRLF,
    'Date: ',
    'Mon, 23 May ',
    '2005 22:38:34',
    ' GMT',
    CRLF,
    CRLF
  ],
  'medium-req': [
    'POST /it HTTP/1.1' + CRLF +
    'Content-Type: text/plain' + CRLF +
    'Transfer-Encoding: chunked' + CRLF +
    CRLF +
    '3' + CRLF +
    '123' + CRLF +
    '6' + CRLF +
    '123456' + CRLF +
    'A' + CRLF +
    '1234567890' + CRLF +
    '9' + CRLF +
    '123456789' + CRLF +
    'C' + CRLF +
    '123456789ABC' + CRLF +
    'F' + CRLF +
    '123456789ABCDEF' + CRLF +
    '0' + CRLF
  ],
  'medium-res': [
    'HTTP/1.0 200 OK' + CRLF +
    'Date: Mon, 23 May 2005 22:38:34 GMT' + CRLF +
    'Content-Type: text/plain' + CRLF +
    'Transfer-Encoding: chunked' + CRLF +
    CRLF +
    '3' + CRLF +
    '123' + CRLF +
    '6' + CRLF +
    '123456' + CRLF +
    'A' + CRLF +
    '1234567890' + CRLF +
    '9' + CRLF +
    '123456789' + CRLF +
    'C' + CRLF +
    '123456789ABC' + CRLF +
    'F' + CRLF +
    '123456789ABCDEF' + CRLF +
    '0' + CRLF
  ],
  'medium-req-chunked': [
    'POST /it HTTP/',
    '1.1' + CRLF,
    'Content-Type',
    ': text',
    '/plain',
    CRLF,
    'Transfer-',
    'Encoding: chunked' + CRLF,
    CRLF + '3' + CRLF + '123',
    CRLF + '6' + CRLF + '123456' + CRLF + 'A' + CRLF,
    '12345',
    '67890' + CRLF,
    '9' + CRLF + '123456789' + CRLF,
    'C' + CRLF + '123456789ABC' + CRLF + 'F' + CRLF + '123456789ABCDEF' + CRLF,
    '0' + CRLF
  ],
  'medium-res-chunked': [
    'HTTP/1.0 2',
    '00 OK' + CRLF + 'Date: Mo',
    'n, 23 May 2005 22',
    ':38:34 GMT' + CRLF + 'Content-Type: text',
    '/plain' + CRLF,
    'Transfer-Encoding: chu',
    'nked' + CRLF + CRLF + '3',
    CRLF + '123' + CRLF + '6' + CRLF,
    '123456' + CRLF + 'A' + CRLF + '1234567890' + CRLF,
    '9' + CRLF,
    '123456789' + CRLF,
    'C' + CRLF,
    '123456789ABC' + CRLF,
    'F' + CRLF,
    '123456789ABCDEF' + CRLF + '0' + CRLF
  ],
  'large-req': [
    'POST /foo/bar/baz?quux=42#1337 HTTP/1.0' + CRLF +
    new Array(256).join('X-Filler: 42' + CRLF) + CRLF
  ],
  'large-res': [
    'HTTP/1.1 200 OK' + CRLF +
    'Content-Type: text/nonsense' + CRLF,
    'Content-Length: 3572' + CRLF + CRLF +
    new Array(256).join('X-Filler: 42' + CRLF) + CRLF
  ],
  'large-req-chunked':
    ('POST /foo/bar/baz?quux=42#1337 HTTP/1.0' + CRLF +
    new Array(256).join('X-Filler: 42' + CRLF) + CRLF).match(/.{1,144}/g)
  ,
  'large-res-chunked':
    ('HTTP/1.1 200 OK' + CRLF +
     'Content-Type: text/nonsense' + CRLF,
     'Content-Length: 3572' + CRLF + CRLF +
     new Array(256).join('X-Filler: 42' + CRLF) + CRLF).match(/.{1,144}/g)
  ,
};

function onHeadersComplete(versionMajor, versionMinor, headers, method,
                           url, statusCode, statusMessage, upgrade,
                           shouldKeepAlive) {
}
function onBody(data, start, len) {
}
function onComplete() {
}

function main(conf) {
  var chunks = inputs[conf.type];
  var n = chunks.length;
  var kind = (/\-req\-?/i.exec(conf.type) ? REQUEST : RESPONSE);

  // Convert strings to Buffers first ...
  for (var i = 0; i < n; ++i)
    chunks[i] = new Buffer(chunks[i], 'binary');

  var parser = new HTTPParser(kind);
  parser.onHeaders = onHeadersComplete;
  parser.onBody = onBody;
  parser.onComplete = onComplete;

  // Allow V8 to optimize first ...
  for (var j = 0; j < 1000; ++j) {
    for (var i = 0; i < n; ++i)
      parser.execute(chunks[i]);
  }

  bench.start();
  for (var i = 0; i < n; ++i)
    parser.execute(chunks[i]);
  bench.end(n);
}
