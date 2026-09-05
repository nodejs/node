'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
});

let serverRequests = 0;

const server = http.createServer(async (req, res) => {
  serverRequests++;

  if (serverRequests === 1) {
    try {
      for await (const chunk of req) {
        throw new Error(`payload too large: ${chunk.length}`);
      }
    } catch {
      res.end('payload too large');
    }
    return;
  }

  res.end('ok');
});

server.listen(0, common.mustCall(() => {
  const first = http.request({
    port: server.address().port,
    method: 'POST',
    agent,
  }, common.mustCall((res) => {
    res.resume();

    res.on('end', common.mustCall(() => {
      process.nextTick(common.mustCall(() => {
        const second = http.request({
          port: server.address().port,
          method: 'GET',
          agent,
        }, common.mustCall((res) => {
          second.setTimeout(0);
          assert.strictEqual(second.reusedSocket, true);
          res.setEncoding('utf8');

          let body = '';

          res.on('data', (chunk) => {
            body += chunk;
          });

          res.on('end', common.mustCall(() => {
            assert.strictEqual(body, 'ok');
            assert.strictEqual(serverRequests, 2);

            agent.destroy();
            server.close();
          }));
        }));

        second.setTimeout(common.platformTimeout(1000), () => {
          assert.fail('second keep-alive request timed out');
        });

        second.end();
      }));
    }));
  }));

  first.end(Buffer.alloc(1_000_000));
}));
