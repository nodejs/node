'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const Countdown = require('../common/countdown');

const server = http.createServer(common.mustCall((req, res) => {
  req.resume();
  res.writeHead(200);
  res.write('');
  setTimeout(() => res.end(req.url), 50);
}, 2));

const countdown = new Countdown(2, common.mustCall(() => server.close()));

server.on('connect', common.mustCall((req, socket) => {
  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');
  socket.resume();
  socket.on('end', () => socket.end());
}));

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port,
    method: 'CONNECT',
    path: 'google.com:80'
  });
  req.on('connect', common.mustCall((res, socket) => {
    socket.end();
    socket.on('end', common.mustCall(() => {
      doRequest(0);
      doRequest(1);
    }));
    socket.resume();
  }));
  req.end();
}));

function doRequest(i) {
  http.get({
    port: server.address().port,
    path: `/request${i}`
  }, common.mustCall((res) => {
    let data = '';
    res.setEncoding('utf8');
    res.on('data', (chunk) => data += chunk);
    res.on('end', common.mustCall(() => {
      assert.strictEqual(data, `/request${i}`);
      countdown.dec();
    }));
  }));
}
