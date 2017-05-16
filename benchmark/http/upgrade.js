'use strict';

const common = require('../common.js');
const PORT = common.PORT;
const net = require('net');

const bench = common.createBenchmark(main, {
  n: [5, 1000]
});

const reqData = 'GET / HTTP/1.1\r\n' +
                'Upgrade: WebSocket\r\n' +
                'Connection: Upgrade\r\n' +
                '\r\n' +
                'WjN}|M(6';

const resData = 'HTTP/1.1 101 Web Socket Protocol Handshake\r\n' +
                'Upgrade: WebSocket\r\n' +
                'Connection: Upgrade\r\n' +
                '\r\n\r\n';

function main({ n }) {
  process.env.PORT = PORT;
  var server = require('../fixtures/simple-http-server.js')
    .listen(common.PORT)
    .on('listening', function() {
      bench.start();
      doBench(server.address(), n, function() {
        bench.end(n);
        server.close();
      });
    })
    .on('upgrade', function(req, socket, upgradeHead) {
      socket.resume();
      socket.write(resData);
      socket.end();
    });
}

function doBench(address, count, done) {
  if (count === 0) {
    done();
    return;
  }

  const conn = net.createConnection(address.port);
  conn.write(reqData);
  conn.resume();

  conn.on('end', function() {
    doBench(address, count - 1, done);
  });
}
