'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

let complete;

const server = http.createServer((req, res) => {
  // We should not see the queued /thatotherone request within the server
  // as it should be aborted before it is sent.
  assert.strictEqual(req.url, '/');

  res.writeHead(200);
  res.write('foo');

  complete = complete || function() {
    res.end();
  };
});


server.listen(0, () => {
  const agent = new http.Agent({ maxSockets: 1 });
  assert.strictEqual(Object.keys(agent.sockets).length, 0);

  const options = {
    hostname: 'localhost',
    port: server.address().port,
    method: 'GET',
    path: '/',
    agent: agent
  };

  const req1 = http.request(options);
  req1.on('response', (res1) => {
    assert.strictEqual(Object.keys(agent.sockets).length, 1);
    assert.strictEqual(Object.keys(agent.requests).length, 0);

    const req2 = http.request({
      method: 'GET',
      host: 'localhost',
      port: server.address().port,
      path: '/thatotherone',
      agent: agent
    });
    assert.strictEqual(Object.keys(agent.sockets).length, 1);
    assert.strictEqual(Object.keys(agent.requests).length, 1);

    // TODO(jasnell): This event does not appear to currently be triggered.
    // is this handler actually required?
    req2.on('error', (err) => {
      // This is expected in response to our explicit abort call
      assert.strictEqual(err.code, 'ECONNRESET');
    });

    req2.end();
    req2.abort();

    assert.strictEqual(Object.keys(agent.sockets).length, 1);
    assert.strictEqual(Object.keys(agent.requests).length, 1);

    res1.on('data', (chunk) => complete());

    res1.on('end', common.mustCall(() => {
      setTimeout(common.mustCall(() => {
        assert.strictEqual(Object.keys(agent.sockets).length, 0);
        assert.strictEqual(Object.keys(agent.requests).length, 0);

        server.close();
      }), 100);
    }));
  });

  req1.end();
});
