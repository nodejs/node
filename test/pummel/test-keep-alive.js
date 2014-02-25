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


if (process.platform === 'win32') {
  console.log('skipping this test because there is no wrk on windows');
  process.exit(0);
}

// This test requires the program 'wrk'
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var http = require('http');
var path = require('path');
var url = require('url');

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
  var args = [
    '-c', opts.concurrent || 100,
    '-t', opts.threads || 2,
    '-d', opts.duration || '10s',
  ];

  if (!opts.keepalive) {
    args.push('-H');
    args.push('Connection: close');
  }

  args.push(url.format({ hostname: '127.0.0.1', port: common.PORT, protocol: 'http'}));

  var comm = path.join(__dirname, '..', '..', 'tools', 'wrk', 'wrk');

  //console.log(comm, args.join(' '));

  var child = spawn(comm, args);
  child.stderr.pipe(process.stderr);
  child.stdout.setEncoding('utf8');

  var stdout;

  child.stdout.on('data', function(data) {
    stdout += data;
  });

  child.on('close', function(code, signal) {
    if (code) {
      console.error(code, signal);
      process.exit(code);
      return;
    }

    var matches = /Requests\/sec:\s*(\d+)\./mi.exec(stdout);
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
  runAb({ keepalive: true }, function(reqSec) {
    keepAliveReqSec = reqSec;
    console.log('keep-alive:', keepAliveReqSec, 'req/sec');

    runAb({ keepalive: false }, function(reqSec) {
      normalReqSec = reqSec;
      console.log('normal:' + normalReqSec + ' req/sec');
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(true, normalReqSec > 50);
  assert.equal(true, keepAliveReqSec > 50);
  assert.equal(true, normalReqSec < keepAliveReqSec);
});
