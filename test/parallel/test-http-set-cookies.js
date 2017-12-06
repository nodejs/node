'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const countdown = new Countdown(2, () => server.close());
const server = http.createServer(function(req, res) {
  if (req.url === '/one') {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['content-type', 'text/plain']]);
    res.end('one\n');
  } else {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['set-cookie', 'B'],
                        ['content-type', 'text/plain']]);
    res.end('two\n');
  }
});
server.listen(0);

server.on('listening', function() {
  //
  // one set-cookie header
  //
  http.get({ port: this.address().port, path: '/one' }, function(res) {
    // set-cookie headers are always return in an array.
    // even if there is only one.
    assert.deepStrictEqual(['A'], res.headers['set-cookie']);
    assert.strictEqual('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      countdown.dec();
    });
  });

  // two set-cookie headers

  http.get({ port: this.address().port, path: '/two' }, function(res) {
    assert.deepStrictEqual(['A', 'B'], res.headers['set-cookie']);
    assert.strictEqual('text/plain', res.headers['content-type']);

    res.on('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.on('end', function() {
      countdown.dec();
    });
  });

});
