'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const MAX_BODY_LENGTH = 1024 * 1024;
const BODY = JSON.stringify({
  a: 'x'.repeat(512 * 1024),
  b: 'x'.repeat(512 * 1024),
  c: 'x'.repeat(512 * 1024),
  d: 'x'.repeat(512 * 1024),
}, null, '\t');

const server = http.createServer(common.mustCall((request, response) => {
  let received = 0;

  request.setEncoding('utf8');
  request.on('data', function onData(chunk) {
    received += chunk.length;
    if (received > MAX_BODY_LENGTH) {
      response.writeHead(413, { 'content-length': 0 });
      response.end();
      request.off('data', onData);
      request.destroy();
    }
  });

  request.on('end', common.mustNotCall());
  request.on('close', common.mustCall(() => {
    assert.ok(received > MAX_BODY_LENGTH);
    assert.strictEqual(request.complete, false);
  }));
}));

server.on('clientError', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  let response;
  const req = http.request({
    method: 'POST',
    port: server.address().port,
    headers: {
      'content-type': 'application/json',
    },
  }, common.mustCall((res) => {
    response = res;
    assert.strictEqual(res.statusCode, 413);
    assert.strictEqual(req.writableEnded, true);
  }));

  req.on('error', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    assert(response);
    assert.strictEqual(req.writableFinished, true);
    assert.strictEqual(response.complete, true);
    server.close();
  }));

  // Preserve the ordering from the reported regression: the request body is
  // written and ended before the response is received.
  req.write(BODY);
  req.end();
}));
