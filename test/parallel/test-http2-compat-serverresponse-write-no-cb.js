// Flags: --expose-http2
'use strict';

const { mustCall,
        mustNotCall,
        expectsError,
        hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { throws } = require('assert');
const { createServer, connect } = require('http2');

// Http2ServerResponse.write does not imply there is a callback

const expectedError = expectsError({
  code: 'ERR_HTTP2_STREAM_CLOSED',
  message: 'The stream is already closed'
}, 2);

{
  const server = createServer();
  server.listen(0, mustCall(() => {
    const port = server.address().port;
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.end();
      request.resume();
    }));

    server.once('request', mustCall((request, response) => {
      client.destroy();
      response.stream.session.on('close', mustCall(() => {
        response.on('error', mustNotCall());
        throws(
          () => { response.write('muahaha'); },
          expectsError({
            code: 'ERR_HTTP2_STREAM_CLOSED',
            type: Error,
            message: 'The stream is already closed'
          })
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
      const headers = {
        ':path': '/',
        ':method': 'get',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.end();
      request.resume();
    }));

    server.once('request', mustCall((request, response) => {
      client.destroy();
      response.stream.session.on('close', mustCall(() => {
        response.write('muahaha', mustCall(expectedError));
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
      const headers = {
        ':path': '/',
        ':method': 'get',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.end();
      request.resume();
    }));

    server.once('request', mustCall((request, response) => {
      response.stream.session.on('close', mustCall(() => {
        response.write('muahaha', 'utf8', mustCall(expectedError));
        server.close();
      }));
      client.destroy();
    }));
  }));
}
