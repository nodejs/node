'use strict';
const common = require('../common');
const assert = require('assert');
const { createHook } = require('async_hooks');
const http = require('http');

// Regression test for https://github.com/nodejs/node/issues/19859.
// Checks that matching destroys are emitted when creating new/reusing old http
// parser instances.

const httpParsers = [];
const dupDestroys = [];
const destroyed = [];

createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (type === 'HTTPPARSER') {
      httpParsers.push(asyncId);
    }
  },
  destroy(asyncId) {
    if (destroyed.includes(asyncId)) {
      dupDestroys.push(asyncId);
    } else {
      destroyed.push(asyncId);
    }
  }
}).enable();

const server = http.createServer((req, res) => {
  res.end();
});

server.listen(common.mustCall(() => {
  http.get({ port: server.address().port }, common.mustCall(() => {
    server.close(common.mustCall(() => {
      server.listen(common.mustCall(() => {
        http.get({ port: server.address().port }, common.mustCall(() => {
          server.close(common.mustCall(() => {
            setTimeout(common.mustCall(verify), 200);
          }));
        }));
      }));
    }));
  }));
}));

function verify() {
  assert.strictEqual(httpParsers.length, 4);

  assert.strictEqual(dupDestroys.length, 0);
  httpParsers.forEach((id) => assert.ok(destroyed.includes(id)));
}
