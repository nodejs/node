include("mjsunit");

var port = 12123;
var N = 1000;
var count = 0;

function Ponger (socket) {
  this.encoding = "UTF8";
  this.timeout = 0;

  this.onConnect = function () {
    puts("got socket.");
  };

  this.onReceive = function (data) {
    assertTrue(count <= N);
    stdout.print("-");
    if (/PING/.exec(data)) {
      socket.send("PONG");
    }
  };

  this.onEOF = function () {
    puts("ponger: onEOF");
    socket.send("QUIT");
    socket.close();
  };

  this.onDisconnect = function () {
    puts("ponger: onDisconnect");
    socket.server.close();
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
    count += 1; 
    if (count < N) {
      socket.send("PING");
    } else {
      puts("sending FIN");
      socket.sendEOF();
    }
  };

  this.onEOF = function () {
    puts("pinger: onEOF");
    assertEquals(N, count);
  };
}

function onLoad() {
  var server = new TCPServer(Ponger);
  server.listen(port);

  var client = new TCPConnection(Pinger);
  client.connect(port);
}
