include("mjsunit.js");


var tests_run = 0;

function pingPongTest (port, host, on_complete) {
  var N = 1000;
  var count = 0;
  var sent_final_ping = false;

  var server = new node.tcp.Server(function (socket) {
    assertTrue(socket.remoteAddress !== null);
    assertTrue(socket.remoteAddress !== undefined);
    if (host === "127.0.0.1")
      assertEquals(socket.remoteAddress, "127.0.0.1");
    else if (host == null)
      assertEquals(socket.remoteAddress, "127.0.0.1");

    socket.setEncoding("utf8");
    socket.timeout = 0;

    socket.onReceive = function (data) {
      assertEquals("open", socket.readyState);
      assertTrue(count <= N);
      if (/PING/.exec(data)) {
        socket.send("PONG");
      }
    };

    socket.onEOF = function () {
      assertEquals("writeOnly", socket.readyState);
      socket.close();
    };

    socket.onDisconnect = function (had_error) {
      assertFalse(had_error);
      assertEquals("closed", socket.readyState);
      socket.server.close();
    };
  });
  server.listen(port, host);

  var client = new node.tcp.Connection();
  assertEquals("closed", client.readyState);

  client.setEncoding("utf8");

  client.onConnect = function () {
    assertEquals("open", client.readyState);
    client.send("PING");
  };

  client.onReceive = function (data) {
    assertEquals("PONG", data);
    count += 1; 

    if (sent_final_ping) {
      assertEquals("readOnly", client.readyState);
      return;
    } else {
      assertEquals("open", client.readyState);
    }

    if (count < N) {
      client.send("PING");
    } else {
      sent_final_ping = true;
      client.send("PING");
      client.close();
    }
  };

  client.onDisconnect = function () {
    assertEquals(N+1, count);
    assertTrue(sent_final_ping);
    if (on_complete) on_complete();
    tests_run += 1;
  };

  assertEquals("closed", client.readyState);
  client.connect(port, host);
}

function onLoad () {
  /* All are run at once, so run on different ports */
  pingPongTest(20989, "localhost");
  pingPongTest(20988, null);
  pingPongTest(20997, "::1");
}

function onExit () {
  assertEquals(3, tests_run);
}
