// Verify that the HTTP server implementation handles multiple instances
// of the same header as per RFC2616: joining the handful of fields by ', '
// that support it, and dropping duplicates for other fields.

var common = require('../common');
var assert = require('assert');
var http = require('http');

var srv = http.createServer(function(req, res) {
  assert.equal(req.headers.accept, 'abc, def, ghijklmnopqrst');
  assert.equal(req.headers.host, 'foo');
  assert.equal(req.headers['x-foo'], 'bingo');
  assert.equal(req.headers['x-bar'], 'banjo, bango');

  res.writeHead(200, {'Content-Type' : 'text/plain'});
  res.end('EOF');

  srv.close();
});

srv.listen(common.PORT, function() {
  var hc = http.createClient(common.PORT, 'localhost');
  var hr = hc.request('/',
      [
        ['accept', 'abc'],
        ['accept', 'def'],
        ['Accept', 'ghijklmnopqrst'],
        ['host', 'foo'],
        ['Host', 'bar'],
        ['hOst', 'baz'],
        ['x-foo', 'bingo'],
        ['x-bar', 'banjo'],
        ['x-bar', 'bango']
      ]);
  hr.end();
});
