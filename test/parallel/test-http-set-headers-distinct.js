'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer, request } = require('http');

const server = createServer(
  common.mustCall((req, res) => {
    req.headersDistinct = { 'x-req-a': ['zzz'] };

    // headersDistinct setter should be a No-Op
    assert.deepStrictEqual(req.headersDistinct, {
      'transfer-encoding': [
        'chunked',
      ],
      'connection': [
        'keep-alive',
      ],
      'host': [
        `127.0.0.1:${server.address().port}`,
      ]
    });

    req.on('end', function() {
      res.write('BODY');
      res.end();
    });

    req.resume();
  })
);

server.listen(
  0,
  common.mustCall(() => {
    const req = request(
      {
        host: '127.0.0.1',
        port: server.address().port,
        path: '/',
        method: 'POST',
      },
      common.mustCall((res) => {
        res.on('end', function() {
          server.close();
        });
        res.resume();
      })
    );

    req.write('BODY');
    req.end();
  })
);
