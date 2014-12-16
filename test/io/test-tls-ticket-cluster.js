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
var cluster = require('cluster');
var tls = require('tls');
var fs = require('fs');
var join = require('path').join;

var workerCount = 4;
var expectedReqCount = 16;

if (cluster.isMaster) {
  var reusedCount = 0;
  var reqCount = 0;
  var lastSession = null;
  var shootOnce = false;

  function shoot() {
    console.error('[master] connecting');
    var c = tls.connect(common.PORT, {
      session: lastSession,
      rejectUnauthorized: false
    }, function() {
      lastSession = c.getSession();
      c.end();

      if (++reqCount === expectedReqCount) {
        Object.keys(cluster.workers).forEach(function(id) {
          cluster.workers[id].send('die');
        });
      } else {
        shoot();
      }
    });
  }

  function fork() {
    var worker = cluster.fork();
    var workerReqCount = 0;
    worker.on('message', function(msg) {
      console.error('[master] got %j', msg);
      if (msg === 'reused') {
        ++reusedCount;
      } else if (msg === 'listening' && !shootOnce) {
        shootOnce = true;
        shoot();
      }
    });

    worker.on('exit', function() {
      console.error('[master] worker died');
    });
  }
  for (var i = 0; i < workerCount; i++) {
    fork();
  }

  process.on('exit', function() {
    assert.equal(reqCount, expectedReqCount);
    assert.equal(reusedCount + 1, reqCount);
  });
  return;
}

var keyFile = join(common.fixturesDir, 'agent.key');
var certFile = join(common.fixturesDir, 'agent.crt');
var key = fs.readFileSync(keyFile);
var cert = fs.readFileSync(certFile);
var options = {
  key: key,
  cert: cert
};

var server = tls.createServer(options, function(c) {
  if (c.isSessionReused()) {
    process.send('reused');
  } else {
    process.send('not-reused');
  }
  c.end();
});

server.listen(common.PORT, function() {
  process.send('listening');
});

process.on('message', function listener(msg) {
  console.error('[worker] got %j', msg);
  if (msg === 'die') {
    server.close(function() {
      console.error('[worker] server close');

      process.exit();
    });
  }
});

process.on('exit', function() {
  console.error('[worker] exit');
});
