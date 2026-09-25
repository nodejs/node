'use strict';

// Regression test for a keep-alive socket reuse race condition.
//
// The race is between responseOnEnd() and requestOnFinish(), both of which
// can call responseKeepAlive().  The window is: req.end() has been called,
// the socket write has completed (writableFinished true), but the write
// callback that emits the 'finish' event has not fired yet.
//
// With plain HTTP the window is normally too narrow to hit.  This test
// widens it by delaying every client-socket write *callback* by a few
// milliseconds (the actual I/O is not delayed, so writableFinished becomes
// true while the 'finish'-emitting callback is still pending).
//
// With Expect: 100-continue, the server responds quickly while the client
// delays req.end() just slightly (setTimeout 0), creating the perfect
// timing for the response to arrive in that window.
//
// On unpatched Node, the double responseKeepAlive() call corrupts the
// socket by stripping a subsequent request's listeners and emitting a
// spurious 'free' event, causing requests to hang / time out.

const common = require('../common');
const assert = require('assert');
const http = require('http');

const REQUEST_COUNT = 100;
const agent = new http.Agent({ keepAlive: true, maxSockets: 1 });

// Delay every write *callback* on the client socket so that
// socket.writableLength drops to 0 (writableFinished becomes true) before
// the callback that ultimately emits the 'finish' event fires.  With
// HTTPS the TLS layer provides this gap naturally; for plain HTTP we
// need to create it artificially.
const patchedSockets = new WeakSet();
function patchSocket(socket) {
  if (patchedSockets.has(socket)) return;
  patchedSockets.add(socket);
  const delay = 5;
  const origWrite = socket.write;
  socket.write = function(chunk, encoding, cb) {
    if (typeof encoding === 'function') {
      cb = encoding;
      encoding = null;
    }
    if (typeof cb === 'function') {
      const orig = cb;
      cb = (...args) => setTimeout(() => orig(...args), delay);
    }
    return origWrite.call(this, chunk, encoding, cb);
  };
}

const server = http.createServer(common.mustCall((req, res) => {
  req.on('error', common.mustNotCall());
  res.writeHead(200);
  res.end();
}, REQUEST_COUNT));

server.listen(0, common.mustCall(() => {
  const { port } = server.address();

  async function run() {
    try {
      for (let i = 0; i < REQUEST_COUNT; i++) {
        await sendRequest(port);
      }
    } finally {
      agent.destroy();
      server.close();
    }
  }

  run().then(common.mustCall());
}));

function sendRequest(port) {
  let timeout;
  const promise = new Promise((resolve, reject) => {
    function done(err) {
      clearTimeout(timeout);
      if (err)
        reject(err);
      else
        resolve();
    }

    const req = http.request({
      port,
      host: '127.0.0.1',
      method: 'POST',
      agent,
      headers: {
        'Content-Length': '0',
        'Expect': '100-continue',
      },
    }, common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume();
      res.once('end', done);
      res.once('error', done);
    }));

    req.on('socket', patchSocket);

    timeout = setTimeout(() => {
      const err = new Error('request timed out');
      req.destroy(err);
      done(err);
    }, common.platformTimeout(5000));

    req.once('error', done);

    setTimeout(() => req.end(Buffer.alloc(0)), 0);
  });
  return promise.finally(() => clearTimeout(timeout));
}
