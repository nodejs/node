include("mjsunit");
var port = 8921;

function onLoad () {

  var server = new node.tcp.Server(function (socket) {
    puts("new connection");
    socket.onConnect = function () {
      socket.send("hello\r\n");
    };

    socket.onEOF = function () {
      socket.close();
    };

    socket.onDisconnect = function (had_error) {
      //puts("server had_error: " + JSON.stringify(had_error));
      assertFalse(had_error);
    };
  });
  server.listen(port);

  var count = 0;
  var client = new node.tcp.Connection();
  
  client.setEncoding("UTF8");
  client.onConnect = function () {
    puts("client connected");
  };

  client.onReceive = function (chunk) {
    puts("got msg");
    assertEquals("hello\r\n", chunk);
    client.fullClose();
  };

  client.onDisconnect = function (had_error) {
    assertFalse(had_error);
    puts("client disconnected");
    if (count++ < 5) 
      client.connect(port); // reconnect
    else
      server.close();
  };

  client.connect(port);
}
