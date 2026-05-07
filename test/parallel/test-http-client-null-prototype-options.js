'use strict';

const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  req.on('end', common.mustCall(() => {
    res.end('ok');
  }));
}, 2));

function makeRequest(options, callback) {
  Object.defineProperty(Object.prototype, 'hostname', {
    __proto__: null,
    configurable: true,
    get: common.mustNotCall('get %Object.prototype%.hostname'),
  });

  let req;
  try {
    req = http.request(options, callback);
  } finally {
    delete Object.prototype.hostname;
  }

  req.on('error', common.mustNotCall());
  req.end();
}

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const { address, port } = server.address();

  makeRequest(
    {
      host: address,
      port,
      path: '/',
    },
    common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume();
      res.on('end', common.mustCall());
    }),
  );

  const nullProtoOptions = { __proto__: null, host: address, port, path: '/' };

  assert.strictEqual(Object.getPrototypeOf(nullProtoOptions), null);

  makeRequest(
    nullProtoOptions,
    common.mustCall((res) => {
      assert.strictEqual(res.statusCode, 200);
      res.resume();
      res.on('end', common.mustCall(() => {
        server.close(common.mustCall());
      }));
    }),
  );
}));
