'use strict';

// Regression test for a keep-alive socket reuse race condition.
//
// The race is between responseOnEnd() and requestOnFinish(), both of which
// can call responseKeepAlive().  The window is: req.end() has been called,
// the socket write has completed (writableFinished true), but the write
// callback that emits the 'finish' event has not fired yet.
//
// HTTPS widens this window because the TLS layer introduces async
// indirection between the actual write completion and the JS callback.
//
// With Expect: 100-continue, the server responds quickly while the client
// delays req.end() just slightly (setTimeout 0), creating the perfect
// timing for the response to arrive in that window.
//
// On unpatched Node, the double responseKeepAlive() call corrupts the
// socket by stripping a subsequent request's listeners and emitting a
// spurious 'free' event, causing requests to hang / time out.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');
const fixtures = require('../common/fixtures');

const REQUEST_COUNT = 100;
const agent = new https.Agent({ keepAlive: true, maxSockets: 1 });

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const server = https.createServer({ key, cert }, common.mustCall((req, res) => {
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

    const req = https.request({
      port,
      host: '127.0.0.1',
      rejectUnauthorized: false,
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
