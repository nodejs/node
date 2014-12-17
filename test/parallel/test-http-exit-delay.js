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

var assert = require('assert');
var common = require('../common.js');
var http = require('http');

var start;
var server = http.createServer(function(req, res) {
  req.resume();
  req.on('end', function() {
    res.end('Success');
  });

  server.close();
});

server.listen(common.PORT, 'localhost', function() {
  var interval_id = setInterval(function() {
    start = new Date();
    if (start.getMilliseconds() > 100)
      return;

    console.log(start.toISOString());
    var req = http.request({
      'host': 'localhost',
      'port': common.PORT,
      'agent': false,
      'method': 'PUT'
    });

    req.end('Test');
    clearInterval(interval_id);
  }, 10);
});

process.on('exit', function() {
  var end = new Date();
  console.log(end.toISOString());
  assert.equal(start.getSeconds(), end.getSeconds());
  assert(end.getMilliseconds() < 900);
  console.log('ok');
});
