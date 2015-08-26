var common = require('../common.js');
var HTTPParser = require('_http_parser');
var CRLF = '\r\n';
var REQUEST = HTTPParser.REQUEST;
var RESPONSE = HTTPParser.RESPONSE;

var bench = common.createBenchmark(main, {
  n: [100000],
  type: [
    'small-req',
    'small-res',
    'small-alternate',
    'medium-req',
    'medium-res',
    'medium-alternate',
    'medium-req-chunked',
    'medium-res-chunked',
    'large-req',
    'large-res',
    'large-alternate',
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
  'small-alternate': true,
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
  'medium-alternate': true,
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
  'large-alternate': true,
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

function onHeaders(versionMajor, versionMinor, headers, method, url, statusCode,
                   statusMessage, upgrade, shouldKeepAlive) {
}
function onBody(data, start, len) {
}
function onComplete() {
}

function main(conf) {
  var type = conf.type;
  var chunks = inputs[type];
  var n = +conf.n;
  var nchunks = (chunks !== true ? chunks.length : 1);
  var kind = (/\-req\-?/i.exec(type) ? REQUEST : RESPONSE);
  var altsize = /^([^\-]+)\-alternate$/.exec(type);
  var req;
  var res;

  if (altsize)
    altsize = altsize[1];

  // Convert strings to Buffers first ...
  if (chunks === true) {
    // alternating
    req = new Buffer(inputs[altsize + '-req'].join(''), 'binary');
    res = new Buffer(inputs[altsize + '-res'].join(''), 'binary');
    kind = REQUEST;
  } else {
    for (var i = 0; i < nchunks; ++i)
      chunks[i] = new Buffer(chunks[i], 'binary');
  }

  var parser = new HTTPParser(kind);
  parser.onHeaders = onHeaders;
  parser.onBody = onBody;
  parser.onComplete = onComplete;

  if (altsize) {
    // Allow V8 to optimize first ...
    for (var j = 0; j < 1000; ++j) {
      parser.reinitialize(REQUEST);
      parser.execute(req);
      parser.reinitialize(RESPONSE);
      parser.execute(res);
    }
    bench.start();
    for (var c = 0; c < n; ++c) {
      parser.reinitialize(REQUEST);
      parser.execute(req);
      parser.reinitialize(RESPONSE);
      parser.execute(res);
    }
    bench.end(n * 2);
  } else {
    // Allow V8 to optimize first ...
    for (var j = 0; j < 1000; ++j) {
      for (var i = 0; i < nchunks; ++i)
        parser.execute(chunks[i]);
    }
    bench.start();
    for (var c = 0; c < n; ++c) {
      for (var i = 0; i < nchunks; ++i)
        parser.execute(chunks[i]);
    }
    bench.end(n * nchunks);
  }
}
