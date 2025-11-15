'use strict';
const common = require('../common');
const net = require('net');
const http = require('http');

// These test cases to check socketOnDrain where needPause becomes false.
// When send large response enough to exceed highWaterMark, it expect the socket
// to be paused and res.write would be failed.
// And it should be resumed when outgoingData falls below highWaterMark.

const server = http.createServer(common.mustNotCall()).on('listening', () => {
  const c = net.createConnection(server.address().port, () => {
    c.write('GET / HTTP/1.1\r\n\r\n');
    c.write('GET / HTTP/1.1\r\n\r\n',
            () => setImmediate(() => c.resume()));
    c.end();
  });

  c.on('end', () => {
    server.close();
  });
});

server.listen(0);
