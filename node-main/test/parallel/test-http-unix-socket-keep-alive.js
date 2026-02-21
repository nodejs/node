'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer((req, res) => res.end());

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

server.listen(common.PIPE, common.mustCall(() =>
  asyncLoop(makeKeepAliveRequest, 10, common.mustCall(() =>
    server.getConnections(common.mustSucceed((conns) => {
      assert.strictEqual(conns, 1);
      server.close();
    }))
  ))
));

function asyncLoop(fn, times, cb) {
  fn(function handler() {
    if (--times) {
      setTimeout(() => fn(handler), common.platformTimeout(10));
    } else {
      cb();
    }
  });
}

function makeKeepAliveRequest(cb) {
  http.get({
    socketPath: common.PIPE,
    headers: { connection: 'keep-alive' }
  }, (res) => res.on('data', common.mustNotCall())
    .on('error', assert.fail)
    .on('end', cb)
  );
}
