process.mixin(require("./common"));
net = require("net");

process.Buffer.prototype.toString = function () {
  return this.utf8Slice(0, this.length);
};

var tests_run = 0;

function pingPongTest (port, host, on_complete) {
  var N = 1000;
  var count = 0;
  var sent_final_ping = false;

  var server = net.createServer(function (socket) {
    puts("connection: " + socket.remoteAddress);

    assert.equal(true, socket.remoteAddress !== null);
    assert.equal(true, socket.remoteAddress !== undefined);
    assert.equal(server, socket.server);

    socket.setNoDelay();
    socket.timeout = 0;

    socket.addListener("data", function (data) {
      puts("server got: " + data);
      assert.equal(true, socket.writable);
      assert.equal(true, socket.readable);
      assert.equal(true, count <= N);
      if (/PING/.exec(data)) {
        socket.send("PONG");
      }
    });

    socket.addListener("eof", function () {
      assert.equal(true, socket.writable);
      assert.equal(false, socket.readable);
      socket.close();
    });

    socket.addListener("close", function () {
      assert.equal(false, socket.writable);
      assert.equal(false, socket.readable);
      socket.server.close();
    });
  });
  server.listen(port, host);

  var client = net.createConnection(port, host);

  client.addListener("connect", function () {
    assert.equal(true, client.readable);
    assert.equal(true, client.writable);
    client.send("PING");
  });

  client.addListener("data", function (data) {
    puts("client got: " + data);

    assert.equal("PONG", data);
    count += 1;

    if (sent_final_ping) {
      assert.equal(false, client.writable);
      assert.equal(true, client.readable);
      return;
    } else {
      assert.equal(true, client.writable);
      assert.equal(true, client.readable);
    }

    if (count < N) {
      client.send("PING");
    } else {
      sent_final_ping = true;
      client.send("PING");
      client.close();
    }
  });

  client.addListener("close", function () {
    assert.equal(N+1, count);
    assert.equal(true, sent_final_ping);
    if (on_complete) on_complete();
    tests_run += 1;
  });
}

/* All are run at once, so run on different ports */
pingPongTest(20989, "localhost");
pingPongTest(20988, null);
pingPongTest(20997, "::1");

process.addListener("exit", function () {
  assert.equal(3, tests_run);
});
