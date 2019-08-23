'use strict';

const common = require('../common');
const { createServer, request } = require('http');

{
  const server = createServer((req, res) => {
    server.close();

    res.end();
    res.writeContinue();

    res.on('error', common.expectsError({
      code: 'ERR_STREAM_WRITE_AFTER_END'
    }));
  });

  server.listen(0, common.mustCall(() => {
    const req = request({
      host: 'localhost',
      port: server.address().port
    });

    req.on('response', common.mustCall((res) => res.resume()));

    req.end();
  }));
}
