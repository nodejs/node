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
const Countdown = require('../common/countdown');
const assert = require('assert');
const http = require('http');

const N = 4;
const M = 4;
const server = http.Server(common.mustCall(function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
}, (N * M))); // N * M = good requests (the errors will not be counted)

function makeRequests(outCount, inCount, shouldFail) {
  const countdown = new Countdown(
    outCount * inCount,
    common.mustCall(() => server.close())
  );
  let onRequest = common.mustNotCall(); // Temporary
  const p = new Promise((resolve) => {
    onRequest = common.mustCall((res) => {
      if (countdown.dec() === 0) {
        resolve();
      }

      if (!shouldFail)
        res.resume();
    }, outCount * inCount);
  });

  server.listen(0, () => {
    const port = server.address().port;
    for (let i = 0; i < outCount; i++) {
      setTimeout(() => {
        for (let j = 0; j < inCount; j++) {
          const req = http.get({ port: port, path: '/' }, onRequest);
          if (shouldFail)
            req.on('error', common.mustCall(onRequest));
          else
            req.on('error', (e) => assert.fail(e));
        }
      }, i);
    }
  });
  return p;
}

const test1 = makeRequests(N, M);

const test2 = () => {
  // Should not explode if can not create sockets.
  // Ref: https://github.com/nodejs/node/issues/13045
  // Ref: https://github.com/nodejs/node/issues/13831
  http.Agent.prototype.createConnection = function createConnection(_, cb) {
    process.nextTick(cb, new Error('nothing'));
  };
  return makeRequests(N, M, true);
};

test1
  .then(test2)
  .catch((e) => {
    // This is currently the way to fail a test with a Promise.
    console.error(e);
    process.exit(1);
  }
  );
