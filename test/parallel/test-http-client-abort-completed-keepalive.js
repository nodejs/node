'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Aborting a request whose exchange has already completed must not destroy the
// socket that is being released to the agent. socketErrorListener has been
// removed by responseKeepAlive() at that point, so the error would be emitted
// on a socket with no 'error' listener and crash the process.
// Refs: https://github.com/nodejs/node/issues/65938

const agent = new http.Agent({ keepAlive: true });

const server = http.createServer((req, res) => {
  res.end('x');
});

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const controller = new AbortController();

  const req = http.get({
    port: server.address().port,
    host: '127.0.0.1',
    agent,
    signal: controller.signal,
  }, common.mustCall(async (res) => {
    res.on('error', common.mustNotCall());

    for await (const chunk of res) {
      assert.strictEqual(chunk.length, 1);
      assert.strictEqual(res.complete, true);
      assert.strictEqual(req.writableFinished, true);
      controller.abort(new Error('stop reading'));
      break;
    }

    // The socket must survive the abort and go back to the pool, and a
    // subsequent request must be able to reuse it.
    const res2 = await new Promise((resolve, reject) => {
      const req2 = http.get({
        port: server.address().port,
        host: '127.0.0.1',
        agent,
      }, resolve);
      req2.on('error', reject);
    });

    let body = '';
    for await (const chunk of res2) body += chunk;
    assert.strictEqual(body, 'x');

    agent.destroy();
    server.close();
  }));

  req.on('error', common.mustNotCall());
}));
