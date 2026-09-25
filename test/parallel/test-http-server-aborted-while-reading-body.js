'use strict';

// Regression test: the request object's 'aborted' event (and .aborted
// property) must be emitted when the client destroys the connection while
// the server is still in the middle of reading the request body, i.e.
// before 'end' has fired on the request stream. This is the "classic"
// abort case and complements
// test-http-server-aborted-after-request-end.js, which covers aborting
// *after* the body has already been fully read.
// See https://github.com/nodejs/node/issues/40775.

const common = require('../common');
const assert = require('assert');
const http = require('http');

let clientReq;
const server = http.createServer(common.mustCall((req, res) => {
  // Close the server once the request stream is torn down, regardless of
  // whether 'aborted' fired. This is guaranteed to happen (unlike
  // 'aborted', which is exactly what's under test here), so a regression
  // makes this test fail fast via the mustCall() check below instead of
  // hanging forever.
  req.on('close', common.mustCall(() => {
    server.close();
  }));

  req.on('aborted', common.mustCall(() => {
    assert.strictEqual(req.aborted, true);
    assert.strictEqual(req.complete, false);
  }));

  req.on('data', common.mustCall(() => {
    // Once the first chunk of the body has arrived, destroy the client
    // connection before it finishes sending the rest of the body (and
    // before the server ever calls res.end()/sends a response).
    clientReq.destroy();
  }));
}));

server.listen(0, common.mustCall(() => {
  clientReq = http.request({
    method: 'POST',
    port: server.address().port,
    headers: { connection: 'keep-alive' },
  }, common.mustNotCall());

  // Destroying the request may or may not surface an error on the client
  // side depending on timing; only the server-side 'aborted' event
  // (asserted above) is under test here.
  clientReq.on('error', () => {});

  // Write a chunk but never call end(), so the server never sees the body
  // finish before the connection is destroyed.
  clientReq.write('some data');
}));
