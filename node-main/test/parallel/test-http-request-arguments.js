'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

// Test providing both a url and options, with the options partially
// replacing address and port portions of the URL provided.
{
  const server = http.createServer(
    common.mustCall((req, res) => {
      assert.strictEqual(req.url, '/testpath');
      res.end();
      server.close();
    })
  );
  server.listen(
    0,
    common.mustCall(() => {
      http.get(
        'http://example.com/testpath',
        { hostname: 'localhost', port: server.address().port },
        common.mustCall((res) => {
          res.resume();
        })
      );
    })
  );
}
