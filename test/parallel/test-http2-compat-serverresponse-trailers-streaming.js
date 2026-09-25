'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Regression test for the auto-empty-trailers optimization: when the
// response headers are flushed before the response is ended (streaming
// mode), trailers registered afterwards must still be sent, and responses
// that never register trailers must complete normally.

const server = h2.createServer();
server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  server.on('request', (request, response) => {
    if (request.url === '/trailers') {
      response.writeHead(200);
      response.write('hello');
      // Trailers registered after the headers were already flushed.
      response.setTrailer('x-checksum', 'abc');
      response.addTrailers({ 'x-count': 2 });
      response.end('world');
    } else {
      response.writeHead(200);
      response.write('no');
      response.end('trailers');
    }
  });

  const client = h2.connect(`http://localhost:${port}`);

  {
    const request = client.request({ ':path': '/trailers' });
    let body = '';
    request.setEncoding('utf8');
    request.on('data', (chunk) => body += chunk);
    request.on('trailers', common.mustCall((trailers) => {
      assert.strictEqual(trailers['x-checksum'], 'abc');
      assert.strictEqual(trailers['x-count'], '2');
    }));
    request.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'helloworld');
      maybeClose();
    }));
    request.end();
  }

  {
    const request = client.request({ ':path': '/plain' });
    let body = '';
    request.setEncoding('utf8');
    request.on('data', (chunk) => body += chunk);
    request.on('trailers', common.mustNotCall());
    request.on('end', common.mustCall(() => {
      assert.strictEqual(body, 'notrailers');
      maybeClose();
    }));
    request.end();
  }

  let remaining = 2;
  function maybeClose() {
    if (--remaining === 0) {
      client.close();
      server.close();
    }
  }
}));
