// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Errors should not be reported both in Http2ServerRequest
// and Http2ServerResponse

{
  let expected = null;

  const server = h2.createServer();

  server.on('stream', common.mustCall(function(stream) {
    stream.on('error', common.mustCall(function(err) {
      assert.strictEqual(err, expected);
    }));

    stream.resume();
    stream.write('hello');

    expected = new Error('kaboom');
    stream.destroy(expected);
    server.close();
  }));

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    const url = `http://localhost:${port}`;
    const client = h2.connect(url, common.mustCall(function() {
      const headers = {
        ':path': '/foobar',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`,
      };
      const request = client.request(headers);
      request.on('data', common.mustCall(function(chunk) {
        // cause an error on the server side
        client.destroy();
      }));
      request.end();
    }));
  }));
}

{
  let expected = null;

  const server = h2.createServer();

  process.on('uncaughtException', common.mustCall(function(err) {
    assert.strictEqual(err.message, 'kaboom no handler');
  }));

  server.on('stream', common.mustCall(function(stream) {
    // there is no 'error'  handler, and this will crash
    stream.write('hello');
    stream.resume();

    expected = new Error('kaboom no handler');
    stream.destroy(expected);
    server.close();
  }));

  server.listen(0, common.mustCall(function() {
    const port = server.address().port;

    const url = `http://localhost:${port}`;
    const client = h2.connect(url, common.mustCall(function() {
      const headers = {
        ':path': '/foobar',
        ':method': 'GET',
        ':scheme': 'http',
        ':authority': `localhost:${port}`,
      };
      const request = client.request(headers);
      request.on('data', common.mustCall(function(chunk) {
        // cause an error on the server side
        client.destroy();
      }));
      request.end();
    }));
  }));
}
