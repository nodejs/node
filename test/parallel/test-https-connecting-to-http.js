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




// This tests the situation where you try to connect a https client
// to an http server. You should get an error and exit.
var common = require('../common');
var assert = require('assert');
var https = require('https');
var http = require('http');


var reqCount = 0;
var resCount = 0;
var reqErrorCount = 0;
var body = 'hello world\n';


var server = http.createServer(function(req, res) {
  reqCount++;
  console.log('got request');
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});


server.listen(common.PORT, function() {
  var req = https.get({ port: common.PORT }, function(res) {
    resCount++;
  });

  req.on('error', function(e) {
    console.log('Got expected error: ', e.message);
    server.close();
    reqErrorCount++;
  });
});


process.on('exit', function() {
  assert.equal(0, reqCount);
  assert.equal(0, resCount);
  assert.equal(1, reqErrorCount);
});
