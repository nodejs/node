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

var assert = require('assert');
var http = require('http');
var util = require('util');

var body = 'hello world';

var server = http.createServer(function(req, res) {
  res.writeHeader(200,
                  {'Content-Length': body.length.toString(),
                    'Content-Type': 'text/plain'
                  });
  console.log('method: ' + req.method);
  if (req.method != 'HEAD') res.write(body);
  res.end();
});
server.listen(common.PORT);

var gotEnd = false;

server.addListener('listening', function() {
  var client = http.createClient(common.PORT);
  var request = client.request('HEAD', '/');
  request.addListener('response', function(response) {
    console.log('got response');
    response.addListener('data', function() {
      process.exit(2);
    });
    response.addListener('end', function() {
      process.exit(0);
    });
  });
  request.end();
});

//give a bit of time for the server to respond before we check it
setTimeout(function() {
  process.exit(1);
}, 2000);
