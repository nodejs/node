include("mjsunit.js");
var N = 50;
var port = 8921;

var disconnect_count = 0;
var client_recv_count = 0;

function onLoad () {

  var server = node.tcp.createServer(function (socket) {
    socket.addListener("connect", function () {
      socket.send("hello\r\n");
    });

    socket.addListener("eof", function () {
      socket.close();
    });

    socket.addListener("disconnect", function (had_error) {
      //puts("server had_error: " + JSON.stringify(had_error));
      assertFalse(had_error);
    });
  });
  server.listen(port);
  var client = new node.tcp.Connection();
  
  client.setEncoding("UTF8");
  client.addListener("connect", function () {
  });

  client.addListener("receive", function (chunk) {
    client_recv_count += 1;
    assertEquals("hello\r\n", chunk);
    client.fullClose();
  });

  client.addListener("disconnect", function (had_error) {
    assertFalse(had_error);
    if (disconnect_count++ < N) 
      client.connect(port); // reconnect
    else
      server.close();
  });

  client.connect(port);
}

function onExit () {
  assertEquals(N+1, disconnect_count);
  assertEquals(N+1, client_recv_count);
}
