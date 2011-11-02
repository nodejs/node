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




// Uploading a big file via HTTPS causes node to drop out of the event loop.
// https://github.com/joyent/node/issues/892
// In this test we set up an HTTPS in this process and launch a subprocess
// to POST a 32mb file to us. A bug in the pause/resume functionality of the
// TLS server causes the child process to exit cleanly before having sent
// the entire buffer.
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var https = require('https');
var fs = require('fs');

var PORT = 8000;


var bytesExpected = 1024 * 1024 * 32;
var gotResponse = false;

var started = false;

var childScript = require('path').join(common.fixturesDir, 'GH-892-request.js');

function makeRequest() {
  if (started) return;
  started = true;

  var stderrBuffer = '';

  var child = spawn(process.execPath,
      [childScript, common.PORT, bytesExpected]);

  child.on('exit', function(code) {
    assert.ok(/DONE/.test(stderrBuffer));
    assert.equal(0, code);
  });

  // The following two lines forward the stdio from the child
  // to parent process for debugging.
  child.stderr.pipe(process.stderr);
  child.stdout.pipe(process.stdout);


  // Buffer the stderr so that we can check that it got 'DONE'
  child.stderr.setEncoding('ascii');
  child.stderr.on('data', function(d) {
    stderrBuffer += d;
  });
}


var serverOptions = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var uploadCount = 0;

var server = https.Server(serverOptions, function(req, res) {
  // Close the server immediately. This test is only doing a single upload.
  // We need to make sure the server isn't keeping the event loop alive
  // while the upload is in progress.
  server.close();

  req.on('data', function(d) {
    process.stderr.write('.');
    uploadCount += d.length;
  });

  req.on('end', function() {
    assert.equal(bytesExpected, uploadCount);
    res.writeHead(200, {'content-type': 'text/plain'});
    res.end('successful upload\n');
  });
});

server.listen(common.PORT, function() {
  console.log('expecting %d bytes', bytesExpected);
  makeRequest();
});

process.on('exit', function() {
  console.error('got %d bytes', uploadCount);
  assert.equal(uploadCount, bytesExpected);
});
