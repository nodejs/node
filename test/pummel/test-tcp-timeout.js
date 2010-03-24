require("../common");
net = require("net");
exchanges = 0;
starttime = null;
timeouttime = null;
timeout = 1000;

var echo_server = net.createServer(function (socket) {
  socket.setTimeout(timeout);

  socket.addListener("timeout", function () {
    puts("server timeout");
    timeouttime = new Date;
    p(timeouttime);
  });

  socket.addListener("data", function (d) {
    puts(d);
    socket.write(d);
  });

  socket.addListener("end", function () {
    socket.close();
  });
});

echo_server.listen(PORT);
puts("server listening at " + PORT);

var client = net.createConnection(PORT);
client.setEncoding("UTF8");
client.setTimeout(0); // disable the timeout for client
client.addListener("connect", function () {
  puts("client connected.");
  client.write("hello\r\n");
});

client.addListener("data", function (chunk) {
  assert.equal("hello\r\n", chunk);
  if (exchanges++ < 5) {
    setTimeout(function () {
      puts("client write 'hello'");
      client.write("hello\r\n");
    }, 500);

    if (exchanges == 5) {
      puts("wait for timeout - should come in " + timeout + " ms");
      starttime = new Date;
      p(starttime);
    }
  }
});

client.addListener("timeout", function () {
  puts("client timeout - this shouldn't happen");
  assert.ok(false);
});

client.addListener("end", function () {
  puts("client end");
  client.close();
});

client.addListener("close", function () {
  puts("client disconnect");
  echo_server.close();
});

process.addListener("exit", function () {
  assert.ok(starttime != null);
  assert.ok(timeouttime != null);

  diff = timeouttime - starttime;
  puts("diff = " + diff);

  assert.ok(timeout < diff);

  // Allow for 800 milliseconds more
  assert.ok(diff < timeout + 800);
});
