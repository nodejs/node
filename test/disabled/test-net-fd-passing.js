var common = require('../common');
var assert = require('assert');
var net = require('net');

var tests_run = 0;

function fdPassingTest(path, port) {
  var greeting = 'howdy';
  var message = 'beep toot';
  var expectedData = ['[greeting] ' + greeting, '[echo] ' + message];

  var receiverArgs = [common.fixturesDir + '/net-fd-passing-receiver.js',
                      path,
                      greeting];
  var receiver = process.createChildProcess(process.ARGV[0], receiverArgs);

  var initializeSender = function() {
    var fdHighway = new net.Socket();

    fdHighway.addListener('connect', function() {
      var sender = net.createServer(function(socket) {
        fdHighway.sendFD(socket);
        socket.flush();
        socket.forceClose(); // want to close() the fd, not shutdown()
      });

      sender.addListener('listening', function() {
        var client = net.createConnection(port);

        client.addListener('connect', function() {
          client.write(message);
        });

        client.addListener('data', function(data) {
          assert.equal(expectedData[0], data);
          if (expectedData.length > 1) {
            expectedData.shift();
          }
          else {
            // time to shut down
            fdHighway.close();
            sender.close();
            client.forceClose();
          }
        });
      });

      tests_run += 1;
      sender.listen(port);
    });

    fdHighway.connect(path);


  };

  receiver.addListener('output', function(data) {
    var initialized = false;
    if ((! initialized) && (data == 'ready')) {
      initializeSender();
      initialized = true;
    }
  });
}

fdPassingTest('/tmp/passing-socket-test', 31075);

process.addListener('exit', function() {
  assert.equal(1, tests_run);
});
