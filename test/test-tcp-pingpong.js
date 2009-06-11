include("mjsunit.js");

var port = 12123;
var N = 1000;
var count = 0;

var sent_final_ping = false;

function Ponger (socket) {
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
    assertEquals("127.0.0.1", socket.remoteAddress);
    assertFalse(had_error);
    assertEquals("closed", socket.readyState);
    socket.server.close();
  };
}

function onLoad() {
  var server = new node.tcp.Server(Ponger);
  server.listen(port);

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
  
  client.onEOF = function () {
  };

  assertEquals("closed", client.readyState);
  client.connect(port, "localhost");
}

function onExit () {
  assertEquals(N+1, count);
  assertTrue(sent_final_ping);
}
