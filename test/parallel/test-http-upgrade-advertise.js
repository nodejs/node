'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

const tests = [
  { headers: {}, expected: 'regular' },
  { headers: { upgrade: 'h2c' }, expected: 'regular' },
  { headers: { connection: 'upgrade' }, expected: 'regular' },
  { headers: { connection: 'upgrade', upgrade: 'h2c' }, expected: 'upgrade' }
];

function fire() {
  if (tests.length === 0)
    return server.close();

  const test = tests.shift();

  const done = common.mustCall(function done(result) {
    assert.strictEqual(result, test.expected);

    fire();
  });

  const req = http.request({
    port: server.address().port,
    path: '/',
    headers: test.headers
  }, function onResponse(res) {
    res.resume();
    done('regular');
  });

  req.on('upgrade', function onUpgrade(res, socket) {
    socket.destroy();
    done('upgrade');
  });

  req.end();
}

const server = http.createServer(function(req, res) {
  res.writeHead(200, {
    Connection: 'upgrade, keep-alive',
    Upgrade: 'h2c'
  });
  res.end('hello world');
}).on('upgrade', function(req, socket) {
  socket.end('HTTP/1.1 101 Switching protocols\r\n' +
             'Connection: upgrade\r\n' +
             'Upgrade: h2c\r\n\r\n' +
             'ohai');
}).listen(0, fire);
