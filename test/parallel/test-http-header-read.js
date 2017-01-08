'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

// Verify that ServerResponse.getHeader() works correctly even after
// the response header has been sent. Issue 752 on github.

const s = http.createServer(function(req, res) {
  const contentType = 'Content-Type';
  const plain = 'text/plain';
  res.setHeader(contentType, plain);
  assert.ok(!res.headersSent);
  res.writeHead(200);
  assert.ok(res.headersSent);
  res.end('hello world\n');
  // This checks that after the headers have been sent, getHeader works
  // and does not throw an exception (Issue 752)
  assert.doesNotThrow(
      function() {
        assert.equal(plain, res.getHeader(contentType));
      }
  );
});

s.listen(0, runTest);

function runTest() {
  http.get({ port: this.address().port }, function(response) {
    response.on('end', function() {
      s.close();
    });
    response.resume();
  });
}
