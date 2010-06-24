require("../common");
net = require("net");
N = 200;

server = net.createServer(function (connection) {
  function write (j) {
    if (j >= N) {
      connection.end();
      return;
    }
    setTimeout(function () {
      connection.write("C");
      write(j+1);
    }, 10);
  }
  write(0);
});
server.listen(PORT);


recv = "";
chars_recved = 0;

client = net.createConnection(PORT);
client.setEncoding("ascii");
client.addListener("data", function (d) {
    print(d);
    recv += d;
});

setTimeout(function () {
  chars_recved = recv.length;
  console.log("pause at: " + chars_recved);
  assert.equal(true, chars_recved > 1);
  client.pause();
  setTimeout(function () {
    console.log("resume at: " + chars_recved);
    assert.equal(chars_recved, recv.length);
    client.resume();

    setTimeout(function () {
      chars_recved = recv.length;
      console.log("pause at: " + chars_recved);
      client.pause();

      setTimeout(function () {
        console.log("resume at: " + chars_recved);
        assert.equal(chars_recved, recv.length);
        client.resume();

      }, 500);

    }, 500);

  }, 500);

}, 500);

client.addListener("end", function () {
  server.close();
  client.end();
});

process.addListener("exit", function () {
  assert.equal(N, recv.length);
  debug("Exit");
});
