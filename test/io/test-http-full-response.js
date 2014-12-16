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
// This test requires the program 'ab'
var http = require('http');
var exec = require('child_process').exec;

var bodyLength = 12345;

var body = '';
for (var i = 0; i < bodyLength; i++) {
  body += 'c';
}

var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': bodyLength,
    'Content-Type': 'text/plain'
  });
  res.end(body);
});

var runs = 0;

function runAb(opts, callback) {
  var command = 'ab ' + opts + ' http://127.0.0.1:' + common.PORT + '/';
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (/ab|apr/mi.test(stderr)) {
        console.log('problem spawning ab - skipping test.\n' + stderr);
        process.reallyExit(0);
      }
      process.exit();
      return;
    }

    var m = /Document Length:\s*(\d+) bytes/mi.exec(stdout);
    var documentLength = parseInt(m[1]);

    var m = /Complete requests:\s*(\d+)/mi.exec(stdout);
    var completeRequests = parseInt(m[1]);

    var m = /HTML transferred:\s*(\d+) bytes/mi.exec(stdout);
    var htmlTransfered = parseInt(m[1]);

    assert.equal(bodyLength, documentLength);
    assert.equal(completeRequests * documentLength, htmlTransfered);

    runs++;

    if (callback) callback();
  });
}

server.listen(common.PORT, function() {
  runAb('-c 1 -n 10', function() {
    console.log('-c 1 -n 10 okay');

    runAb('-c 1 -n 100', function() {
      console.log('-c 1 -n 100 okay');

      runAb('-c 1 -n 1000', function() {
        console.log('-c 1 -n 1000 okay');
        server.close();
      });
    });
  });

});

process.on('exit', function() {
  assert.equal(3, runs);
});
