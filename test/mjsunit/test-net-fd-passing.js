process.mixin(require("./common"));
net = require("net");

process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

var tests_run = 0;

function fdPassingTest(path, port) {
  var greeting = "howdy";
  var message = "beep toot";
  var expectedData = ["[greeting] " + greeting, "[echo] " + message];

  puts(fixturesDir);
  var receiverArgs = [fixturesDir + "/net-fd-passing-receiver.js", path, greeting];
  var receiver = process.createChildProcess(process.ARGV[0], receiverArgs);

  var initializeSender = function() {
    var fdHighway = new net.Socket();
    fdHighway.connect(path);

    var sender = net.createServer(function(socket) {
      fdHighway.sendFD(socket);
      socket.flush();
      socket.forceClose(); // want to close() the fd, not shutdown()
    });

    sender.addListener("listening", function() {
      var client = net.createConnection(port);

      client.addListener("connect", function() {
        client.send(message);
      });

      client.addListener("data", function(data) {
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
  };

  receiver.addListener("output", function(data) {
    var initialized = false;
    if ((! initialized) && (data == "ready")) {
      initializeSender();
      initialized = true;
    }
  });
}

fdPassingTest("/tmp/passing-socket-test", 31075);

process.addListener("exit", function () {
  assert.equal(1, tests_run);
});
