'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const { strictEqual } = require('assert');
const h2 = require('http2');

// Http2ServerResponse.finished cannot be true before res.end() is
// called or the request is aborted.
const server = h2
  .createServer()
  .listen(0, common.mustCall(function() {
    const port = server.address().port;
    server.once('request', common.mustCall((req, res) => {
      strictEqual(res.finished, false);
      res.writeHead(400);
      strictEqual(res.finished, false);
      strictEqual(res.headersSent, true);
      res.end();
      strictEqual(res.finished, true);
    }));

    const url = `http://localhost:${port}`;
    const client = h2.connect(url, common.mustCall(function() {
      const headers = {
        ':path': '/',
        ':method': 'HEAD',
        ':scheme': 'http',
        ':authority': `localhost:${port}`
      };
      const request = client.request(headers);
      request.on('end', common.mustCall(function() {
        client.close();
        server.close();
      }));
      request.end();
      request.resume();
    }));
  }));
