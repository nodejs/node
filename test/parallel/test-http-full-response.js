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

'use strict';
const common = require('../common');
const assert = require('assert');
// This test requires the program 'ab'
const http = require('http');
const exec = require('child_process').exec;

const bodyLength = 12345;

const body = 'c'.repeat(bodyLength);

const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Length': bodyLength,
    'Content-Type': 'text/plain'
  });
  res.end(body);
});

function runAb(opts, callback) {
  const command = `ab ${opts} http://127.0.0.1:${server.address().port}/`;
  exec(command, function(err, stdout, stderr) {
    if (err) {
      if (/ab|apr/i.test(stderr)) {
        common.printSkipMessage(`problem spawning \`ab\`.\n${stderr}`);
        process.reallyExit(0);
      }
      throw err;
    }

    let m = /Document Length:\s*(\d+) bytes/i.exec(stdout);
    const documentLength = parseInt(m[1]);

    m = /Complete requests:\s*(\d+)/i.exec(stdout);
    const completeRequests = parseInt(m[1]);

    m = /HTML transferred:\s*(\d+) bytes/i.exec(stdout);
    const htmlTransferred = parseInt(m[1]);

    assert.strictEqual(bodyLength, documentLength);
    assert.strictEqual(completeRequests * documentLength, htmlTransferred);

    if (callback) callback();
  });
}

server.listen(0, common.mustCall(function() {
  runAb('-c 1 -n 10', common.mustCall(function() {
    console.log('-c 1 -n 10 okay');

    runAb('-c 1 -n 100', common.mustCall(function() {
      console.log('-c 1 -n 100 okay');

      runAb('-c 1 -n 1000', common.mustCall(function() {
        console.log('-c 1 -n 1000 okay');
        server.close();
      }));
    }));
  }));
}));
