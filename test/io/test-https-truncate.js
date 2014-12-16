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
var https = require('https');
var path = require('path');

var resultFile = path.resolve(common.tmpDir, 'result');

var key = fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem');
var cert = fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem');

var PORT = common.PORT;

// number of bytes discovered empirically to trigger the bug
var data = new Buffer(1024 * 32 + 1);

httpsTest();

function httpsTest() {
  var sopt = { key: key, cert: cert };

  var server = https.createServer(sopt, function(req, res) {
    res.setHeader('content-length', data.length);
    res.end(data);
    server.close();
  });

  server.listen(PORT, function() {
    var opts = { port: PORT, rejectUnauthorized: false };
    https.get(opts).on('response', function(res) {
      test(res);
    });
  });
}


function test(res) {
  res.on('end', function() {
    assert.equal(res._readableState.length, 0);
    assert.equal(bytes, data.length);
    console.log('ok');
  });

  // Pause and then resume on each chunk, to ensure that there will be
  // a lone byte hanging out at the very end.
  var bytes = 0;
  res.on('data', function(chunk) {
    bytes += chunk.length;
    this.pause();
    setTimeout(this.resume.bind(this));
  });
}
