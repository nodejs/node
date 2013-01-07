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

var common = require('../common');
var assert = require('assert');
var http = require('http');

var ntimeouts = 0;
var nchunks = 0;

process.on('exit', function() {
  assert.equal(ntimeouts, 1);
  assert.equal(nchunks, 2);
});

var options = {
  method: 'GET',
  port: common.PORT,
  host: '127.0.0.1',
  path: '/'
};

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Length':'2'});
  res.write('*');
  setTimeout(function() { res.end('*') }, 100);
});

server.listen(options.port, options.host, function() {
  var req = http.request(options, onresponse);
  req.end();

  function onresponse(res) {
    req.setTimeout(50, function() {
      assert.equal(nchunks, 1); // should have received the first chunk by now
      ntimeouts++;
    });

    res.on('data', function(data) {
      assert.equal('' + data, '*');
      nchunks++;
    });

    res.on('end', function() {
      assert.equal(nchunks, 2);
      server.close();
    });
  }
});
