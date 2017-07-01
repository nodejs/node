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
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const cluster = require('cluster');
const fs = require('fs');
const join = require('path').join;

const workerCount = 4;
const expectedReqCount = 16;

if (cluster.isMaster) {
  let reusedCount = 0;
  let reqCount = 0;
  let lastSession = null;
  let shootOnce = false;
  let workerPort = null;

  function shoot() {
    console.error('[master] connecting', workerPort);
    const c = tls.connect(workerPort, {
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
    const worker = cluster.fork();
    worker.on('message', function({ msg, port }) {
      console.error('[master] got %j', msg);
      if (msg === 'reused') {
        ++reusedCount;
      } else if (msg === 'listening' && !shootOnce) {
        workerPort = port || workerPort;
        shootOnce = true;
        shoot();
      }
    });

    worker.on('exit', function() {
      console.error('[master] worker died');
    });
  }
  for (let i = 0; i < workerCount; i++) {
    fork();
  }

  process.on('exit', function() {
    assert.strictEqual(reqCount, expectedReqCount);
    assert.strictEqual(reusedCount + 1, reqCount);
  });
  return;
}

const keyFile = join(common.fixturesDir, 'agent.key');
const certFile = join(common.fixturesDir, 'agent.crt');
const key = fs.readFileSync(keyFile);
const cert = fs.readFileSync(certFile);
const options = {
  key: key,
  cert: cert
};

const server = tls.createServer(options, function(c) {
  if (c.isSessionReused()) {
    process.send({ msg: 'reused' });
  } else {
    process.send({ msg: 'not-reused' });
  }
  c.end();
});

server.listen(0, function() {
  const { port } = server.address();
  process.send({
    msg: 'listening',
    port,
  });
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
