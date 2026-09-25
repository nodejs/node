'use strict';
// Flags: --expose-gc
const common = require('../common');
const http = require('http');
const assert = require('assert');
const { onGC } = require('../common/gc');

function createServer() {
  const server = http.createServer(common.mustCall((req, res) => {
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify({ hello: 'world' }));
    req.socket.write('HTTP/1.1 400 Bad Request\r\n\r\n');
  }));

  return new Promise((resolve) => {
    server.listen(0, common.mustCall(() => {
      resolve(server);
    }));
  });
}

async function main() {
  const server = await createServer();
  const req = http.get({
    port: server.address().port,
  }, common.mustCall((res) => {
    const chunks = [];
    res.on('data', common.mustCallAtLeast((c) => chunks.push(c), 1));
    res.on('end', common.mustCall(() => {
      const body = Buffer.concat(chunks).toString('utf8');
      const data = JSON.parse(body);
      assert.strictEqual(data.hello, 'world');
    }));
  }));
  const timer = setInterval(global.gc, 300);
  onGC(req, {
    ongc: common.mustCall(() => {
      clearInterval(timer);
      server.close();
    })
  });
}

main();
