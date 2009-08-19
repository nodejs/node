include("mjsunit.js");
PORT = 20443;
N = 500;

server = node.tcp.createServer(function (connection) {
  function send (j) {
    if (j >= N) { 
      connection.close();
      return;
    }
    setTimeout(function () {
      connection.send("C");
      send(j+1);
    }, 10);
  }
  send(0);
});
server.listen(PORT);


recv = "";
chars_recved = 0;

function onLoad () {
  client = node.tcp.createConnection(PORT);
  client.setEncoding("ascii");
  client.addListener("receive", function (d) {
      print(d);
      recv += d;
  });

  setTimeout(function () {
    chars_recved = recv.length; 
    puts("pause at: " + chars_recved);
    assertTrue(chars_recved > 1);
    client.readPause();
    setTimeout(function () {
      puts("resume at: " + chars_recved);
      assertEquals(chars_recved, recv.length);
      client.readResume();

      setTimeout(function () {
        chars_recved = recv.length; 
        puts("pause at: " + chars_recved);
        client.readPause();

        setTimeout(function () {
          puts("resume at: " + chars_recved);
          assertEquals(chars_recved, recv.length);
          client.readResume();

        }, 500);

      }, 500);

    }, 500);

  }, 500);

  client.addListener("eof", function () {
    server.close();
    client.close();
  });
}

function onExit () {
  assertEquals(N, recv.length);
}
