process.mixin(require("./common"));
tcp = require("tcp");


var tests_run = 0;

function pingPongTest (port, host, on_complete) {
  var N = 1000;
  var count = 0;
  var sent_final_ping = false;

  var server = tcp.createServer(function (socket) {
    assert.equal(true, socket.remoteAddress !== null);
    assert.equal(true, socket.remoteAddress !== undefined);
    if (host === "127.0.0.1")
      assert.equal(socket.remoteAddress, "127.0.0.1");
    else if (host == null)
      assert.equal(socket.remoteAddress, "127.0.0.1");

    socket.setEncoding("utf8");
    socket.setNoDelay();
    socket.timeout = 0;

    socket.addListener("data", function (data) {
      puts("server got: " + JSON.stringify(data));
      assert.equal("open", socket.readyState);
      assert.equal(true, count <= N);
      if (/PING/.exec(data)) {
        socket.write("PONG");
      }
    });

    socket.addListener("end", function () {
      assert.equal("writeOnly", socket.readyState);
      socket.close();
    });

    socket.addListener("close", function (had_error) {
      assert.equal(false, had_error);
      assert.equal("closed", socket.readyState);
      socket.server.close();
    });
  });
  server.listen(port, host);

  var client = tcp.createConnection(port, host);

  client.setEncoding("utf8");

  client.addListener("connect", function () {
    assert.equal("open", client.readyState);
    client.write("PING");
  });

  client.addListener("data", function (data) {
    assert.equal("PONG", data);
    count += 1;

    if (sent_final_ping) {
      assert.equal("readOnly", client.readyState);
      return;
    } else {
      assert.equal("open", client.readyState);
    }

    if (count < N) {
      client.write("PING");
    } else {
      sent_final_ping = true;
      client.write("PING");
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
