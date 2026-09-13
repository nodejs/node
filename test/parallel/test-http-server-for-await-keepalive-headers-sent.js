'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
});

let serverRequests = 0;
let firstResponseReceived = false;
let firstResponseConnection;
let firstResponseAborted = false;
let firstResponseClosed = false;
let secondStarted = false;

const server = http.createServer(async (req, res) => {
  serverRequests++;

  if (serverRequests === 1) {
    res.write('partial');

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
  const port = server.address().port;

  function startSecondRequest() {
    if (secondStarted) {
      return;
    }

    secondStarted = true;

    process.nextTick(() => {
      const second = http.request({
        port,
        method: 'GET',
        agent,
      }, common.mustCall((res) => {
        second.setTimeout(0);

        assert.strictEqual(second.reusedSocket, false);

        if (firstResponseReceived) {
          assert.strictEqual(firstResponseConnection, 'keep-alive');
          assert.strictEqual(firstResponseAborted, true);
          assert.strictEqual(firstResponseClosed, true);
        }

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

      second.setTimeout(
        common.platformTimeout(1000),
        common.mustNotCall('second request timed out'),
      );

      second.end();
    });
  }

  const first = http.request({
    port,
    method: 'POST',
    agent,
  }, (res) => {
    firstResponseReceived = true;
    firstResponseConnection = res.headers.connection;

    res.on('end', common.mustNotCall());
    res.on('aborted', () => {
      firstResponseAborted = true;
    });
    res.on('error', common.expectsError({
      code: 'ECONNRESET',
      message: 'aborted',
    }));

    res.on('close', () => {
      firstResponseClosed = true;
      startSecondRequest();
    });

    res.resume();
  });

  first.on('error', (err) => {
    switch (err.code) {
      case 'ECONNRESET':
      case 'ECONNABORTED':
      case 'EPIPE':
        break;
      default:
        throw err;
    }

    if (!firstResponseReceived) {
      startSecondRequest();
    }
  });

  first.end(Buffer.alloc(1_000_000));
}));
