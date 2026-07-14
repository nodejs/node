'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const BODY = Buffer.alloc(2 * 1024 * 1024);

const server = http.createServer(common.mustCall((request, response) => {
  let received = 0;

  request.on('data', function onData(chunk) {
    received += chunk.length;
    if (received > BODY.length / 2) {
      request.off('data', onData);
      response.writeHead(413, { 'content-length': 0 });
      response.end();
      request.destroy();
    }
  });

  request.on('end', common.mustNotCall());
  request.on('close', common.mustCall(() => {
    assert.strictEqual(request.complete, false);
    assert.ok(received > 0);
  }));
}));

server.listen(0, common.mustCall(() => {
  let response;
  const req = http.request({
    method: 'POST',
    port: server.address().port,
    headers: {
      'content-type': 'application/json',
      'content-length': BODY.length,
    },
  }, common.mustCall((res) => {
    response = res;
    assert.strictEqual(res.statusCode, 413);
    // Deliberately do not consume the response body. A response that has
    // completed after the request finished should not be followed by a late
    // ClientRequest socket error.
  }));

  req.on('error', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    assert(response);
    assert.strictEqual(req.writableFinished, true);
    assert.strictEqual(response.complete, true);
    server.close();
  }));

  req.end(BODY);
}));
