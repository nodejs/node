// Flags: --expose-http2
'use strict';

const { mustCall, mustNotCall, hasCrypto, skip } = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { strictEqual } = require('assert');
const {
  createServer,
  connect,
  constants: {
    HTTP2_HEADER_STATUS,
    HTTP_STATUS_OK
  }
} = require('http2');

{
  // Http2ServerResponse.end callback is called only the first time,
  // but may be invoked repeatedly without throwing errors.
  const server = createServer(mustCall((request, response) => {
    strictEqual(response.closed, false);
    response.end(mustCall(() => {
      server.close();
    }));
    response.end(mustNotCall());
    strictEqual(response.closed, true);
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => client.destroy()));
      request.end();
      request.resume();
    }));
  }));
}

{
  // Http2ServerResponse.end is not necessary on HEAD requests since the stream
  // is already closed. Headers, however, can still be sent to the client.
  const server = createServer(mustCall((request, response) => {
    strictEqual(response.finished, true);
    response.writeHead(HTTP_STATUS_OK, { foo: 'bar' });
    response.end(mustNotCall());
  }));
  server.listen(0, mustCall(() => {
    const { port } = server.address();
    const url = `http://localhost:${port}`;
    const client = connect(url, mustCall(() => {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('response', mustCall((headers, flags) => {
        strictEqual(headers[HTTP2_HEADER_STATUS], HTTP_STATUS_OK);
        strictEqual(flags, 5); // the end of stream flag is set
        strictEqual(headers.foo, 'bar');
      }));
      request.on('data', mustNotCall());
      request.on('end', mustCall(() => {
        client.destroy();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
}
