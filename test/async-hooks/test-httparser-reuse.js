'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');
const { createHook } = require('async_hooks');
const reused = Symbol('reused');

let reusedHTTPParser = false;
const asyncHook = createHook({
  init(asyncId, type, triggerAsyncId, resource) {
    if (resource[reused]) {
      reusedHTTPParser = true;
    }
    resource[reused] = true;
  }
});
asyncHook.enable();

const server = http.createServer(function(req, res) {
  res.end();
});

const PORT = 3000;
const url = 'http://127.0.0.1:' + PORT;

server.listen(PORT, common.mustCall(() => {
  http.get(url, common.mustCall(() => {
    server.close(common.mustCall(() => {
      server.listen(PORT, common.mustCall(() => {
        http.get(url, common.mustCall(() => {
          server.close(common.mustCall(() => {
            assert.strictEqual(reusedHTTPParser, false);
          }));
        }));
      }));
    }));
  }));
}));
