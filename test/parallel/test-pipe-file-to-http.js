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
var fs = require('fs');
var http = require('http');
var path = require('path');
var cp = require('child_process');

var filename = path.join(common.tmpDir || '/tmp', 'big');
var clientReqComplete = false;
var count = 0;

var server = http.createServer(function(req, res) {
  console.error('SERVER request');
  var timeoutId;
  assert.equal('POST', req.method);
  req.pause();
  common.error('request paused');

  setTimeout(function() {
    req.resume();
    common.error('request resumed');
  }, 1000);

  req.on('data', function(chunk) {
    common.error('recv data! nchars = ' + chunk.length);
    count += chunk.length;
  });

  req.on('end', function() {
    if (timeoutId) {
      clearTimeout(timeoutId);
    }
    console.log('request complete from server');
    res.writeHead(200, {'Content-Type': 'text/plain'});
    res.end();
  });
});
server.listen(common.PORT);

server.on('listening', function() {
  var cmd = common.ddCommand(filename, 10240);
  console.log('dd command: ', cmd);

  cp.exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    console.error('EXEC returned successfully stdout=%d stderr=%d',
                  stdout.length, stderr.length);
    makeRequest();
  });
});

function makeRequest() {
  var req = http.request({
    port: common.PORT,
    path: '/',
    method: 'POST'
  });

  common.error('pipe!');

  var s = fs.ReadStream(filename);
  s.pipe(req);
  s.on('data', function(chunk) {
    console.error('FS data chunk=%d', chunk.length);
  });
  s.on('end', function() {
    console.error('FS end');
  });
  s.on('close', function(err) {
    if (err) throw err;
    clientReqComplete = true;
    common.error('client finished sending request');
  });

  req.on('response', function(res) {
    console.error('RESPONSE', res.statusCode, res.headers);
    res.resume();
    res.on('end', function() {
      console.error('RESPONSE end');
      server.close();
    });
  });
}

process.on('exit', function() {
  assert.equal(1024 * 10240, count);
  assert.ok(clientReqComplete);
});
