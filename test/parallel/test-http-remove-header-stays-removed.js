'use strict';
require('../common');
const assert = require('assert');

const http = require('http');

const server = http.createServer(function(request, response) {
  // removed headers should stay removed, even if node automatically adds them
  // to the output:
  response.removeHeader('connection');
  response.removeHeader('transfer-encoding');
  response.removeHeader('content-length');

  // make sure that removing and then setting still works:
  response.removeHeader('date');
  response.setHeader('date', 'coffee o clock');

  response.end('beep boop\n');

  this.close();
});

let response = '';

process.on('exit', function() {
  assert.strictEqual('beep boop\n', response);
  console.log('ok');
});

server.listen(0, function() {
  http.get({ port: this.address().port }, function(res) {
    assert.strictEqual(200, res.statusCode);
    assert.deepStrictEqual(res.headers, { date: 'coffee o clock' });

    res.setEncoding('ascii');
    res.on('data', function(chunk) {
      response += chunk;
    });
  });
});
