common = require("../common");
assert = common.assert

fs = require("fs");
dgram = require("dgram");

serverPath = "/tmp/dgram_server_sock";
clientPath = "/tmp/dgram_client_sock";
msgToSend = new Buffer("A message to send");

server = dgram.createSocket("unix_dgram");
server.on("message", function (msg, rinfo) {
  console.log("server got: " + msg + " from " + rinfo.address);
  assert.strictEqual(rinfo.address, clientPath);
  assert.strictEqual(msg.toString(), msgToSend.toString());
  server.send(msg, 0, msg.length, rinfo.address);
});

server.on("listening", function () {
  console.log("server is listening");

  client = dgram.createSocket("unix_dgram");

  client.on("message", function (msg, rinfo) {
    console.log("client got: " + msg + " from " + rinfo.address);
    assert.strictEqual(rinfo.address, serverPath);
    assert.strictEqual(msg.toString(), msgToSend.toString());
    client.close();
    server.close();
  });

  client.on("listening", function () {
    console.log("client is listening");
    client.send(msgToSend, 0, msgToSend.length, serverPath, function (err, bytes) {
      if (err) {
        console.log("Caught error in client send.");
        throw err;
      }
      console.log("client wrote " + bytes + " bytes.");
      assert.strictEqual(bytes, msgToSend.length);
    });
  });


  client.bind(clientPath);
});

server.bind(serverPath);
