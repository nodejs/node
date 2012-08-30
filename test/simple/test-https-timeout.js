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

if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var exec = require('child_process').exec;
var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

// a server that never replies
var server = https.createServer(options, function() {
  console.log('Got request.  Doing nothing.');
}).listen(common.PORT, function() {
  var req = https.request({
    host: 'localhost',
    port: common.PORT,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  });
  req.setTimeout(10);
  req.end();

  req.on('response', function(res) {
    console.log('got response');
  });

  req.on('socket', function() {
    console.log('got a socket');

    req.socket.on('connect', function() {
      console.log('socket connected');
    });

    setTimeout(function() {
      throw new Error('Did not get timeout event');
    }, 200);
  });

  req.on('timeout', function() {
    console.log('timeout occurred outside');
    req.destroy();
    server.close();
    process.exit(0);
  });
});
