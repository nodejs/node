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
    server.close();

    req.socket.destroy();

    const onError = common.expectsError({
      code: 'ERR_STREAM_DESTROYED'
    });

    res.on('error', (err) => onError(err));
    res.write('hello');
  });

  server.listen(0, common.mustCall(() => {
    const req = request({
      host: 'localhost',
      port: server.address().port
    });

    req.on('response', common.mustNotCall());
    req.on('error', common.mustCall());

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
      res.write('world');
    });

    res.on('error', (err) => {
      onError(err);
      server.close();
    });
  });

  server.listen(0, common.mustCall(() => {
    const req = request({
      host: 'localhost',
      port: server.address().port
    });

    req.on('response', common.mustCall(() => {
      req.socket.destroy();
    }));

    req.end();
  }));
}
