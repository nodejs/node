var common = require('../common');
var assert = require('assert');
var http = require('http');

var CRLF = '\r\n';

var server = http.createServer();
server.on('upgrade', function(req, socket, head) {
  socket.write('HTTP/1.1 101 Ok' + CRLF +
               'Connection: Upgrade' + CRLF +
               'Upgrade: Test' + CRLF + CRLF + 'head');
  socket.on('end', function() {
    socket.end();
  });
});

var successCount = 0;

server.listen(common.PORT, function() {

  var client = http.createClient(common.PORT);

  function upgradeRequest(fn) {
    console.log("req");
    var header = { 'Connection': 'Upgrade', 'Upgrade': 'Test' };
    var request = client.request('GET', '/', header);
    var wasUpgrade = false;

    function onUpgrade(res, socket, head) {
      console.log("client upgraded");
      wasUpgrade = true;

      client.removeListener('upgrade', onUpgrade);
      socket.end();
    }
    client.on('upgrade', onUpgrade);

    function onEnd() {
      console.log("client end");
      client.removeListener('end', onEnd);
      if (!wasUpgrade) {
        throw new Error('hasn\'t received upgrade event');
      } else {
        fn && process.nextTick(fn);
      }
    }
    client.on('end', onEnd);

    request.write('head');

  }

  upgradeRequest(function() {
    successCount++;
    upgradeRequest(function() {
      successCount++;
      // Test pass
      console.log('Pass!');
      client.end();
      client.destroy();
      server.close();
    });
  });

});

process.on('exit', function() {
  assert.equal(2, successCount);
});
