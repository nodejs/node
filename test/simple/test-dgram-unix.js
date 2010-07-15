common = require("../common");
assert = common.assert

var Buffer = require("buffer").Buffer,
    fs = require("fs"),
    dgram = require("dgram"), server, client,
    server_path = "/tmp/dgram_server_sock",
    client_path = "/tmp/dgram_client_sock",
    message_to_send = new Buffer("A message to send"),
    timer;

server = dgram.createSocket("unix_dgram");
server.on("message", function (msg, rinfo) {
  console.log("server got: " + msg + " from " + rinfo.address);
  assert.strictEqual(rinfo.address, client_path);
  assert.strictEqual(msg.toString(), message_to_send.toString());
  server.send(msg, 0, msg.length, rinfo.address);
});
server.on("listening", function () {
  console.log("server is listening");
  client = dgram.createSocket("unix_dgram");
  client.on("message", function (msg, rinfo) {
    console.log("client got: " + msg + " from " + rinfo.address);
    assert.strictEqual(rinfo.address, server_path);
    assert.strictEqual(msg.toString(), message_to_send.toString());
    client.close();
    server.close();
  });
  client.on("listening", function () {
    console.log("client is listening");
    client.send(message_to_send, 0, message_to_send.length, server_path, function (err, bytes) {
      if (err) {
        console.log("Caught error in client send.");
        throw err;
      }
      console.log("client wrote " + bytes + " bytes.");
      assert.strictEqual(bytes, message_to_send.length);
    });
  });
  client.on("close", function () {
    if (server.fd === null) {
      clearTimeout(timer);
    }
  });
  client.bind(client_path);
});
server.on("close", function () {
  if (client.fd === null) {
    clearTimeout(timer);
  }
});
server.bind(server_path);

timer = setTimeout(function () {
    throw new Error("Timeout");
}, 200);
