'use strict';

const common = require('../common');
const http = require('http');
const assert = require('assert');

// TODO(@jasnell) At some point this should be refactored as the API should not
// be allowing users to set multiple content-length values in the first place.

function test(server) {
  server.listen(0, common.mustCall(() => {
    http.get(
      { port: server.address().port },
      () => { assert.fail('Client allowed multiple content-length headers.'); }
    ).on('error', common.mustCall((err) => {
      assert.ok(err.message.startsWith('Parse Error'), err.message);
      assert.strictEqual(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
      server.close();
    }));
  }));
}

// Test adding an extra content-length header using setHeader().
{
  const server = http.createServer((req, res) => {
    res.setHeader('content-length', [2, 1]);
    res.end('ok');
  });

  test(server);
}

// Test adding an extra content-length header using writeHead().
{
  const server = http.createServer((req, res) => {
    res.writeHead(200, { 'content-length': [1, 2] });
    res.end('ok');
  });

  test(server);
}
