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

// http://groups.google.com/group/nodejs/browse_thread/thread/f66cd3c960406919
var common = require('../common');
var assert = require('assert');
var http = require('http');
var cp = require('child_process');
var fs = require('fs');

var filename = require('path').join(common.tmpDir, 'big');

var count = 0;
function maybeMakeRequest() {
  if (++count < 2) return;
  console.log('making curl request');
  var cmd = 'curl http://127.0.0.1:' + common.PORT + '/ | openssl sha1';
  cp.exec(cmd, function(err, stdout, stderr) {
    if (err) throw err;
    var hex = stdout.match(/([A-Fa-f0-9]{40})/)[0];
    assert.equal('8c206a1a87599f532ce68675536f0b1546900d7a', hex);
    console.log('got the correct response');
    fs.unlink(filename);
    server.close();
  });
}


var ddcmd = common.ddCommand(filename, 10240);
console.log('dd command: ', ddcmd);

cp.exec(ddcmd, function(err, stdout, stderr) {
  if (err) throw err;
  maybeMakeRequest();
});


var server = http.createServer(function(req, res) {
  res.writeHead(200);

  // Create the subprocess
  var cat = cp.spawn('cat', [filename]);

  // Stream the data through to the response as binary chunks
  cat.stdout.on('data', function(data) {
    res.write(data);
  });

  // End the response on exit (and log errors)
  cat.on('exit', function(code) {
    if (code !== 0) {
      console.error('subprocess exited with code ' + code);
      exit(1);
    }
    res.end();
  });

});

server.listen(common.PORT, maybeMakeRequest);

console.log('Server running at http://localhost:8080');
