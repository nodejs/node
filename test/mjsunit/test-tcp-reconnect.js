process.mixin(require("./common"));
tcp = require("tcp");
var N = 50;
var port = 8921;

var c = 0;
var client_recv_count = 0;
var disconnect_count = 0;

var server = tcp.createServer(function (socket) {
  socket.addListener("connect", function () {
    socket.write("hello\r\n");
  });

  socket.addListener("end", function () {
    socket.close();
  });

  socket.addListener("close", function (had_error) {
    //puts("server had_error: " + JSON.stringify(had_error));
    assert.equal(false, had_error);
  });
});
server.listen(port);

var client = tcp.createConnection(port);

client.setEncoding("UTF8");

client.addListener("connect", function () {
  puts("client connected.");
});

client.addListener("data", function (chunk) {
  client_recv_count += 1;
  puts("client_recv_count " + client_recv_count);
  assert.equal("hello\r\n", chunk);
  client.close();
});

client.addListener("close", function (had_error) {
  puts("disconnect");
  assert.equal(false, had_error);
  if (disconnect_count++ < N)
    client.connect(port); // reconnect
  else
    server.close();
});

process.addListener("exit", function () {
  assert.equal(N+1, disconnect_count);
  assert.equal(N+1, client_recv_count);
});
