'use strict';

const {
  mustCall,
  mustNotCall,
  expectsError,
  hasCrypto,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');
const { createServer, connect } = require('http2');
const assert = require('assert');

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
    // response.write() returns true
    assert(response.write('muahaha', 'utf8', mustCall()));

    response.stream.close(0, mustCall(() => {
      response.on('error', mustNotCall());

      // response.write() without cb returns error
      expectsError(
        () => { response.write('muahaha'); },
        {
          type: Error,
          code: 'ERR_HTTP2_INVALID_STREAM',
          message: 'The stream has been destroyed'
        }
      );

      // response.write() with cb returns falsy value
      assert(!response.write('muahaha', mustCall()));

      client.destroy();
      server.close();
    }));
  }));
}));
