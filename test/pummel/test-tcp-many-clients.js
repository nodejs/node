process.mixin(require("../common"));
tcp = require("tcp");
// settings
var bytes = 1024*40;
var concurrency = 100;
var connections_per_client = 5;

// measured
var total_connections = 0;

var body = "";
for (var i = 0; i < bytes; i++) {
  body += "C";
}

var server = tcp.createServer(function (c) {
  c.addListener("connect", function () {
    total_connections++;
    print("#");
    c.write(body);
    c.close();
  });
});
server.listen(PORT);

function runClient (callback) {
  var client = tcp.createConnection(PORT);
  client.connections = 0;
  client.setEncoding("utf8");

  client.addListener("connect", function () {
    print("c");
    client.recved = "";
    client.connections += 1;
  });

  client.addListener("data", function (chunk) {
    this.recved += chunk;
  });

  client.addListener("end", function (had_error) {
    client.close();
  });

  client.addListener("close", function (had_error) {
    print(".");
    assert.equal(false, had_error);
    assert.equal(bytes, client.recved.length);
    if (this.connections < connections_per_client) {
      this.connect(PORT);
    } else {
      callback();
    }
  });
}


var finished_clients = 0;
for (var i = 0; i < concurrency; i++) {
  runClient(function () {
    if (++finished_clients == concurrency) server.close();
  });
}

process.addListener("exit", function () {
  assert.equal(connections_per_client * concurrency, total_connections);
  puts("\nokay!");
});
