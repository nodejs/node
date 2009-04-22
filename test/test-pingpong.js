include("mjsunit");
var N = 1000;
function onLoad() {
  server = new Server(1024);
  var count = 0;
  server.listenTCP(12123, function (connection) {
    puts("got connection.");
    connection.onRead = function (data) {
      assertTrue(count <= N);
      if (data === null) {
        server.close();
        connection.close();
        return; 
      }
      //stdout.write ("-");
      if (/QUIT/.exec(data)) {
        server.close();
        connection.close();
      } else if (/PING/.exec(data)) {
        connection.write("PONG");
      }
    }
  });

  socket = new Socket;
  socket.onRead = function (data) {
    //stdout.write(".");
    assertEquals("PONG", data);
    setTimeout(function() {
      count += 1; 
      if (count < N) {
        socket.write("PING");
      } else {
        stdout.write ("\n");
        socket.write("QUIT\n");
        socket.close();
      }
    }, 10);
  };

  socket.onClose = function () {
    puts("socket close.");
    assertEquals(N, count);
  };

  socket.connectTCP(12123, "localhost", function (status) {
    if(status != 0)
      process.exit(1);

    socket.write("PING");
  });
}
