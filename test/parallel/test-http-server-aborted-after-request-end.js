'use strict';

// Regression test: the request object's 'aborted' event (and .aborted
// property) must still be emitted/set when the client destroys the
// connection, even if the request body had already been fully consumed
// (i.e. the request stream already emitted 'end') before the response
// finished. This complements test-http-server-aborted-while-reading-body.js,
// which covers aborting *while* the body is still being read.
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
  }));

  req.on('end', common.mustCall(() => {
    // At this point the request body has been fully read, but the
    // response has not been sent yet. Destroying the client request
    // now must still result in the server's 'aborted' event firing.
    clientReq.destroy();
  }));

  req.resume();
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

  clientReq.end();
}));
