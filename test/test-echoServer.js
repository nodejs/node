include("mjsunit");
function onLoad() {
  server = new Server(1024);
  puts("listening at port 12123")
  server.listenTCP(12123, function (connection) {
    puts("got connection.");
    connection.onRead = function (data) {
      if (data === null) {
        server.close();
        connection.close();
        return; 
      }
      puts ("server read: " + data.toString());
      if (/QUIT/.exec(data)) {
        server.close();
        connection.close();
      } else if (/PING/.exec(data)) {
        connection.write("PONG");
      }
    }
  });

  socket = new Socket;

  var count = 0;
  socket.onRead = function (data) {
    puts ("client read: " + data.toString());
    assertEquals("PONG", data);
    setTimeout(function() {
      count += 1; 
      if (count < 10) {
        socket.write("PING");
      } else {
        socket.write("QUIT\n");
        socket.close();
      }
    }, 100);
  };
  socket.onClose = function () {
    assertEquals(10, count);
  }

  socket.connectTCP(12123, "localhost", function (status) {
    if(status != 0)
      process.exit(1);

    socket.write("PING");
  });
}
