'use strict';
const common = require('../common');
const http = require('http');

const CRLF = '\r\n';

const server = http.createServer();
server.on('upgrade', function(req, socket, head) {
  socket.write('HTTP/1.1 101 Ok' + CRLF +
               'Connection: Upgrade' + CRLF +
               'Upgrade: Test' + CRLF + CRLF + 'head');
  socket.on('end', function() {
    socket.end();
  });
});

server.listen(0, common.mustCall(function() {

  function upgradeRequest(fn) {
    console.log('req');
    const header = { 'Connection': 'Upgrade', 'Upgrade': 'Test' };
    const request = http.request({
      port: server.address().port,
      headers: header
    });
    let wasUpgrade = false;

    function onUpgrade(res, socket, head) {
      console.log('client upgraded');
      wasUpgrade = true;

      request.removeListener('upgrade', onUpgrade);
      socket.end();
    }
    request.on('upgrade', onUpgrade);

    function onEnd() {
      console.log('client end');
      request.removeListener('end', onEnd);
      if (!wasUpgrade) {
        throw new Error('hasn\'t received upgrade event');
      } else {
        fn && process.nextTick(fn);
      }
    }
    request.on('close', onEnd);

    request.write('head');

  }

  upgradeRequest(common.mustCall(function() {
    upgradeRequest(common.mustCall(function() {
      // Test pass
      console.log('Pass!');
      server.close();
    }));
  }));
}));
