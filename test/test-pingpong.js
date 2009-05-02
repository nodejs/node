include("mjsunit");

var port = 12123;
var N = 100;
var count = 0;

var server;

function Ponger (socket) {
  this.encoding = "UTF8";

  this.onConnect = function () {
    puts("got socket.");
  };

  this.onReceive = function (data) {
    assertTrue(count <= N);
    stdout.print ("-");
    if (/QUIT/.exec(data)) {
      socket.disconnect();
      //server.close();
    } else if (/PING/.exec(data)) {
      socket.send("PONG");
    }
  };
}

function Pinger (socket) {
  this.encoding = "UTF8";

  this.onConnect = function () {
    socket.send("PING");
  };

  this.onReceive = function (data) {
    stdout.print(".");
    assertEquals("PONG", data);
    setTimeout(function() {
      count += 1; 
      if (count < N) {
        socket.send("PING");
      } else {
        stdout.write("\n");
        socket.send("QUIT\n");
        socket.disconnect();
      }
    }, 10);
  };

  this.onDisconnect = function () {
    puts("socket close.");
    assertEquals(N, count);
    server.close();
  };
}

function onLoad() {
  server = new TCPServer(Ponger);
  server.listen(port);

  var client = new TCPConnection(Pinger);
  client.connect(port);
}
