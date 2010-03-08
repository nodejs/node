require("../common");
tcp = require("tcp");
exchanges = 0;
starttime = null;
timeouttime = null;
timeout = 1000;

var echo_server = tcp.createServer(function (socket) {
  socket.setTimeout(timeout);

  socket.addListener("timeout", function (d) {
    puts("server timeout");
    timeouttime = new Date;
    p(timeouttime);
  });

  socket.addListener("data", function (d) {
    p(d);
    socket.write(d);
  });

  socket.addListener("end", function () {
    socket.close();
  });
});

echo_server.listen(PORT);
puts("server listening at " + PORT);

var client = tcp.createConnection(PORT);
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
  assert.equal(false, true);
});

client.addListener("end", function () {
  puts("client end");
  client.close();
});

client.addListener("close", function (had_error) {
  puts("client disconnect");
  echo_server.close();
  assert.equal(false, had_error);
});

process.addListener("exit", function () {
  assert.equal(true, starttime != null);
  assert.equal(true, timeouttime != null);

  diff = timeouttime - starttime;
  puts("diff = " + diff);
  assert.equal(true, timeout < diff);
  // Allow for 800 milliseconds more
  assert.equal(true, diff < timeout + 800);
});
