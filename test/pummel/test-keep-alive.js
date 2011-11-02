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




// This test requires the program 'ab'
var common = require('../common');
var assert = require('assert');
var http = require('http');
var exec = require('child_process').exec;

var body = 'hello world\n';
var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': body.length,
    'Content-Type': 'text/plain'
  });
  res.write(body);
  res.end();
});

var keepAliveReqSec = 0;
var normalReqSec = 0;


function runAb(opts, callback) {
  var command = 'ab ' + opts + ' http://127.0.0.1:' + common.PORT + '/';
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (stderr.indexOf('ab') >= 0) {
        console.log('ab not installed? skipping test.\n' + stderr);
        process.reallyExit(0);
      }
      return;
    }
    if (err) throw err;
    var matches = /Requests per second:\s*(\d+)\./mi.exec(stdout);
    var reqSec = parseInt(matches[1]);

    matches = /Keep-Alive requests:\s*(\d+)/mi.exec(stdout);
    var keepAliveRequests;
    if (matches) {
      keepAliveRequests = parseInt(matches[1]);
    } else {
      keepAliveRequests = 0;
    }

    callback(reqSec, keepAliveRequests);
  });
}

server.listen(common.PORT, function() {
  runAb('-k -c 100 -t 2', function(reqSec, keepAliveRequests) {
    keepAliveReqSec = reqSec;
    assert.equal(true, keepAliveRequests > 0);
    console.log('keep-alive: ' + keepAliveReqSec + ' req/sec');

    runAb('-c 100 -t 2', function(reqSec, keepAliveRequests) {
      normalReqSec = reqSec;
      assert.equal(0, keepAliveRequests);
      console.log('normal: ' + normalReqSec + ' req/sec');
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(true, normalReqSec > 50);
  assert.equal(true, keepAliveReqSec > 50);
  assert.equal(true, normalReqSec < keepAliveReqSec);
});
