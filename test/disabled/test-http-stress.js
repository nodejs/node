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

var request_count = 1000;
var body = '{"ok": true}';

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/javascript'});
  res.write(body);
  res.end();
});
server.listen(common.PORT);

var requests_ok = 0;
var requests_complete = 0;

server.addListener('listening', function() {
  for (var i = 0; i < request_count; i++) {
    http.cat('http://localhost:' + common.PORT + '/', 'utf8',
             function(err, content) {
               requests_complete++;
               if (err) {
                 common.print('-');
               } else {
                 assert.equal(body, content);
                 common.print('.');
                 requests_ok++;
               }
               if (requests_complete == request_count) {
                 console.log('\nrequests ok: ' + requests_ok);
                 server.close();
               }
             });
  }
});

process.addListener('exit', function() {
  assert.equal(request_count, requests_complete);
  assert.equal(request_count, requests_ok);
});
