'use strict';

const common = require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');
const http = require('http');

// Verify that resource emitted for an HTTPParser is not reused.
// Verify that correct create/destroy events are emitted.

const reused = Symbol('reused');

const reusedParser = [];
const incomingMessageParser = [];
const clientRequestParser = [];
const dupDestroys = [];
const destroyed = [];

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    switch (type) {
      case 'HTTPINCOMINGMESSAGE':
        incomingMessageParser.push(asyncId);
        break;
      case 'HTTPCLIENTREQUEST':
        clientRequestParser.push(asyncId);
        break;
    }

    if (resource[reused]) {
      reusedParser.push(
        `resource reused: ${asyncId}, ${triggerAsyncId}, ${type}`,
      );
    }
    resource[reused] = true;
  },
  destroy(asyncId) {
    if (destroyed.includes(asyncId)) {
      dupDestroys.push(asyncId);
    } else {
      destroyed.push(asyncId);
    }
  },
}).enable();

const server = http.createServer((req, res) => {
  res.end();
});

server.listen(0, common.mustCall(() => {
  const PORT = server.address().port;
  const url = `http://127.0.0.1:${PORT}`;
  http.get(url, common.mustCall(() => {
    server.close(common.mustCall(() => {
      server.listen(PORT, common.mustCall(() => {
        http.get(url, common.mustCall(() => {
          server.close(common.mustCall(() => {
            setTimeout(common.mustCall(verify), 200);
          }));
        }));
      }));
    }));
  }));
}));

function verify() {
  assert.strictEqual(reusedParser.length, 0);

  assert.strictEqual(incomingMessageParser.length, 2);
  assert.strictEqual(clientRequestParser.length, 2);

  assert.strictEqual(dupDestroys.length, 0);
  incomingMessageParser.forEach((id) => assert.ok(destroyed.includes(id)));
  clientRequestParser.forEach((id) => assert.ok(destroyed.includes(id)));
}
