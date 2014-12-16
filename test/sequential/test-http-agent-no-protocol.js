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
var url = require('url');

var request = 0;
var response = 0;
process.on('exit', function() {
  assert.equal(1, request, 'http server "request" callback was not called');
  assert.equal(1, response, 'http client "response" callback was not called');
});

var server = http.createServer(function(req, res) {
  res.end();
  request++;
}).listen(common.PORT, '127.0.0.1', function() {
  var opts = url.parse('http://127.0.0.1:' + common.PORT + '/');

  // remove the `protocol` field… the `http` module should fall back
  // to "http:", as defined by the global, default `http.Agent` instance.
  opts.agent = new http.Agent();
  opts.agent.protocol = null;

  http.get(opts, function (res) {
    response++;
    res.resume();
    server.close();
  });
});
