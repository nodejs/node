include("mjsunit");

var port = 12123;
var N = 1000;
var count = 0;

function Ponger (socket) {
  socket.encoding = "UTF8";
  socket.timeout = 0;

  puts("got socket.");

  socket.onReceive = function (data) {
    assertEquals("open", socket.readyState);
    //puts("server recved data: " + JSON.stringify(data));
    assertTrue(count <= N);
    stdout.print("-");
    if (/PING/.exec(data)) {
      socket.send("PONG");
    }
  };

  socket.onEOF = function () {
    assertEquals("writeOnly", socket.readyState);
    puts("ponger: onEOF");
    socket.close();
  };

  socket.onDisconnect = function () {
    assertEquals("closed", socket.readyState);
    puts("ponger: onDisconnect");
    socket.server.close();
  };
}

function onLoad() {
  var server = new node.tcp.Server(Ponger);
  server.listen(port);

  var client = new node.tcp.Connection();
  assertEquals("closed", client.readyState);

  client.encoding = "UTF8";

  client.onConnect = function () {
    assertEquals("open", client.readyState);
    puts("client is connected.");
    client.send("PING");
  };

  client.onReceive = function (data) {
    assertEquals("open", client.readyState);
    //puts("client recved data: " + JSON.stringify(data));
    stdout.print(".");
    assertEquals("PONG", data);
    count += 1; 
    if (count < N) {
      client.send("PING");
    } else {
      puts("sending FIN");
      client.close();
    }
  };
  
  client.onEOF = function () {
    puts("pinger: onEOF");
    assertEquals(N, count);
  };

  client.connect(port);
}
