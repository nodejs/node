'use strict';

// This tests that `server.close()` marks an active connection (mid-request
// or awaiting a response) to close once its in-flight work completes,
// instead of leaving it open for keep-alive reuse until `keepAliveTimeout`.
// It also checks the response then advertises `Connection: close` instead
// of `keep-alive`.

const common = require('../common');
const assert = require('assert');

const { createServer, get, Agent } = require('http');

const agent = new Agent({ keepAlive: true });

let failTimer;

const server = createServer(
  { keepAliveTimeout: 2000 },
  common.mustCall((req, res) => {
    req.resume();

    failTimer = setTimeout(() => {
      assert.fail(
        `expected server.close() to finish within 1000ms of the response completing,` +
          ` but it seems to be waiting for the keepAliveTimeout to elapse`,
      );
    }, common.platformTimeout(1000));

    server.close(
      common.mustCall(() => {
        clearTimeout(failTimer);
      }),
    );

    setTimeout(
      common.mustCall(() => res.end('ok')),
      common.platformTimeout(500),
    );
  }),
);

server.listen(
  0,
  common.mustCall(() => {
    const port = server.address().port;

    get(
      { port, agent },
      common.mustCall((res) => {
        assert.strictEqual(res.headers.connection, 'close');

        let body = '';
        res.on('data', (chunk) => (body += chunk));
        res.on(
          'end',
          common.mustCall(() => assert.strictEqual(body, 'ok')),
        );
      }),
    );
  }),
);
