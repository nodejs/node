'use strict';
const common = require('../common');
const http = require('http');

const testServer = http.createServer(common.mustNotCall());
testServer.on('connect', common.mustCall((req, socket, head) => {
  socket.write('HTTP/1.1 200 Connection Established\r\n' +
      'Proxy-agent: Node-Proxy\r\n' +
      '\r\n');
  // This shouldn't raise an assertion in StreamBase::Consume.
  testServer.emit('connection', socket);
  testServer.close();
}));
testServer.listen(0, common.mustCall(() => {
  http.request({
    port: testServer.address().port,
    method: 'CONNECT'
  }, (res) => {}).end();
}));
