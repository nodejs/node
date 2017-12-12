'use strict';

const { mustCall,
        mustNotCall,
        expectsError,
        hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { createServer, connect } = require('http2');

// Http2ServerResponse.write does not imply there is a callback

{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const port = server.address().port;
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const request = client.request();
      request.resume();
      request.on('end', mustCall());
      request.on('close', mustCall(() => {
        client.close();
      }));
    }));

    server.once('request', mustCall((request, response) => {
      client.destroy();
      response.stream.session.on('close', mustCall(() => {
        response.on('error', mustNotCall());
        expectsError(
          () => { response.write('muahaha'); },
          {
            code: 'ERR_HTTP2_INVALID_STREAM'
          }
        );
        server.close();
      }));
    }));
  }));
}

{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const port = server.address().port;
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const request = client.request();
      request.resume();
      request.on('end', mustCall());
      request.on('close', mustCall(() => client.close()));
    }));

    server.once('request', mustCall((request, response) => {
      client.destroy();
      response.stream.session.on('close', mustCall(() => {
        expectsError(
          () => response.write('muahaha'),
          {
            code: 'ERR_HTTP2_INVALID_STREAM'
          }
        );
        server.close();
      }));
    }));
  }));
}

{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const port = server.address().port;
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const request = client.request();
      request.resume();
      request.on('end', mustCall());
      request.on('close', mustCall(() => client.close()));
    }));

    server.once('request', mustCall((request, response) => {
      response.stream.session.on('close', mustCall(() => {
        expectsError(
          () => response.write('muahaha', 'utf8'),
          {
            code: 'ERR_HTTP2_INVALID_STREAM'
          }
        );
        server.close();
      }));
      client.destroy();
    }));
  }));
}
