'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// The purpose of this test is to ensure that res.finished is
// not true before res.end() is called or the request is aborted
// for HEAD requests.

const server = h2
  .createServer()
  .listen(0, common.mustCall(function() {
    const port = server.address().port;
    server.once('request', common.mustCall((req, res) => {
      assert.ok(!res.finished); // This fails.
      res.end();
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
      }));
      request.end();
      request.resume();
    }));
  }))
