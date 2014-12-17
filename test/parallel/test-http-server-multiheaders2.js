// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// Verify that the HTTP server implementation handles multiple instances
// of the same header as per RFC2616: joining the handful of fields by ', '
// that support it, and dropping duplicates for other fields.

var common = require('../common');
var assert = require('assert');
var http = require('http');

var multipleAllowed = [
  'Accept',
  'Accept-Charset',
  'Accept-Encoding',
  'Accept-Language',
  'Connection',
  'Cookie',
  'DAV', // GH-2750
  'Pragma', // GH-715
  'Link', // GH-1187
  'WWW-Authenticate', // GH-1083
  'Proxy-Authenticate', // GH-4052
  'Sec-Websocket-Extensions', // GH-2764
  'Sec-Websocket-Protocol', // GH-2764
  'Via', // GH-6660

  // not a special case, just making sure it's parsed correctly
  'X-Forwarded-For',

  // make sure that unspecified headers is treated as multiple
  'Some-Random-Header',
  'X-Some-Random-Header',
];

var multipleForbidden = [
  'Content-Type',
  'User-Agent',
  'Referer',
  'Host',
  'Authorization',
  'Proxy-Authorization',
  'If-Modified-Since',
  'If-Unmodified-Since',
  'From',
  'Location',
  'Max-Forwards',

  // special case, tested differently
  //'Content-Length',
];

var srv = http.createServer(function(req, res) {
  multipleForbidden.forEach(function(header) {
    assert.equal(req.headers[header.toLowerCase()], 'foo', 'header parsed incorrectly: ' + header);
  });
  multipleAllowed.forEach(function(header) {
    assert.equal(req.headers[header.toLowerCase()], 'foo, bar', 'header parsed incorrectly: ' + header);
  });
  assert.equal(req.headers['content-length'], 0);

  res.writeHead(200, {'Content-Type' : 'text/plain'});
  res.end('EOF');

  srv.close();
});

function makeHeader(value) {
  return function(header) {
    return [header, value];
  }
}

var headers = []
  .concat(multipleAllowed.map(makeHeader('foo')))
  .concat(multipleForbidden.map(makeHeader('foo')))
  .concat(multipleAllowed.map(makeHeader('bar')))
  .concat(multipleForbidden.map(makeHeader('bar')))
  // content-length is a special case since node.js
  // is dropping connetions with non-numeric headers
  .concat([['content-length', 0], ['content-length', 123]]);

srv.listen(common.PORT, function() {
  http.get({
    host: 'localhost',
    port: common.PORT,
    path: '/',
    headers: headers,
  });
});
