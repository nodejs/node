'use strict';
// Verify that the HTTP server implementation handles multiple instances
// of the same header as per RFC2616: joining the handful of fields by ', '
// that support it, and dropping duplicates for other fields.

require('../common');
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
    assert.equal(req.headers[header.toLowerCase()],
                 'foo', 'header parsed incorrectly: ' + header);
  });
  multipleAllowed.forEach(function(header) {
    assert.equal(req.headers[header.toLowerCase()],
                 'foo, bar', 'header parsed incorrectly: ' + header);
  });

  res.writeHead(200, {'Content-Type': 'text/plain'});
  res.end('EOF');

  srv.close();
});

function makeHeader(value) {
  return function(header) {
    return [header, value];
  };
}

var headers = []
  .concat(multipleAllowed.map(makeHeader('foo')))
  .concat(multipleForbidden.map(makeHeader('foo')))
  .concat(multipleAllowed.map(makeHeader('bar')))
  .concat(multipleForbidden.map(makeHeader('bar')));

srv.listen(0, function() {
  http.get({
    host: 'localhost',
    port: this.address().port,
    path: '/',
    headers: headers,
  });
});
