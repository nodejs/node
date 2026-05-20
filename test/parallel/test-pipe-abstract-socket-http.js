'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

if (!common.isLinux) common.skip();

const server = http.createServer(
  common.mustCall((req, res) => {
    res.end('ok');
  })
);

server.listen(
  '\0abstract',
  common.mustCall(() => {
    http.get(
      {
        socketPath: server.address(),
      },
      common.mustCall((res) => {
        assert.strictEqual(res.statusCode, 200);
        server.close();
      })
    );
  })
);
