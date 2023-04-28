'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const http = require('http');

const asyncLocalStorage = new AsyncLocalStorage();

const agent = new http.Agent({
  maxSockets: 1,
});

const N = 3;
let responses = 0;

const server = http.createServer(common.mustCall((req, res) => {
  res.end('ok');
}, N));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  for (let i = 0; i < N; i++) {
    asyncLocalStorage.run(i, () => {
      http.get({ agent, port }, common.mustCall((res) => {
        assert.strictEqual(asyncLocalStorage.getStore(), i);
        if (++responses === N) {
          server.close();
          agent.destroy();
        }
        res.resume();
      }));
    });
  }
}));
