'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const BODY = Buffer.alloc(1024 * 1024);

const server = http.createServer(common.mustCall((request, response) => {
  response.writeHead(202, { 'content-length': 0 });
  response.end();

  request.once('data', common.mustCall(() => {
    request.socket.resetAndDestroy();
  }));
}));

server.on('clientError', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  let response;
  const req = http.request({
    method: 'POST',
    port: server.address().port,
    headers: { 'content-length': BODY.length },
  }, common.mustCall((res) => {
    response = res;
    assert.strictEqual(res.statusCode, 202);
    assert.strictEqual(req.writableEnded, false);
    req.write(BODY);
  }));

  req.on('error', common.mustCall((err) => {
    assert(response);
    assert.strictEqual(response.complete, true);
    assert.strictEqual(req.writableFinished, false);
    switch (err.code) {
      case 'ECONNRESET':
      case 'ECONNABORTED':
      case 'EPIPE':
        break;
      default:
        assert.fail(`Unexpected error code ${err.code}`);
    }
  }));

  req.on('close', common.mustCall(() => server.close()));
  req.flushHeaders();
}));
