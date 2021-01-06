'use strict';

const common = require('../common');
const { createServer, request } = require('http');

{
  const server = createServer((req, res) => {
    server.close();

    req.socket.destroy();

    res.write('hello', common.expectsError({
      code: 'ERR_STREAM_DESTROYED'
    }));
  });

  server.listen(0, common.mustCall(() => {
    const req = request({
      host: 'localhost',
      port: server.address().port
    });

    req.on('response', common.mustNotCall());
    req.on('error', common.expectsError({
      code: 'ECONNRESET'
    }));

    req.end();
  }));
}

{
  const server = createServer((req, res) => {
    res.write('hello');
    req.resume();

    const onError = common.expectsError({
      code: 'ERR_STREAM_DESTROYED'
    });

    res.on('close', () => {
      res.write('world', common.mustCall((err) => {
        onError(err);
        server.close();
      }));
    });
  });

  server.listen(0, common.mustCall(() => {
    const req = request({
      host: 'localhost',
      port: server.address().port
    });

    req.on('response', common.mustCall((res) => {
      res.socket.destroy();
    }));

    req.end();
  }));
}
