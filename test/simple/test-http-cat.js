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

var body = 'exports.A = function() { return "A";}';
var server = http.createServer(function(req, res) {
  console.log('got request');
  res.writeHead(200, [
    ['Content-Length', body.length],
    ['Content-Type', 'text/plain']
  ]);
  res.end(body);
});

var got_good_server_content = false;
var bad_server_got_error = false;

server.listen(common.PORT, function() {
  http.cat('http://localhost:' + common.PORT + '/', 'utf8',
           function(err, content) {
             if (err) {
               throw err;
             } else {
               console.log('got response');
               got_good_server_content = true;
               assert.equal(body, content);
               server.close();
             }
           });

  http.cat('http://localhost:12312/', 'utf8', function(err, content) {
    if (err) {
      console.log('got error (this should happen)');
      bad_server_got_error = true;
    }
  });
});

process.addListener('exit', function() {
  console.log('exit');
  assert.equal(true, got_good_server_content);
  assert.equal(true, bad_server_got_error);
});
