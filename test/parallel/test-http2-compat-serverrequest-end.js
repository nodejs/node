// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

// Http2ServerRequest should always end readable stream
// even on GET requests with no body

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    request.on('data', () => {});
    request.on('end', common.mustCall(() => {
      response.on('finish', common.mustCall(function() {
        server.close();
      }));
      response.end();
    }));
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.resume();
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
  }));
}));
