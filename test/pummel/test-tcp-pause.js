process.mixin(require("../common"));
tcp = require("tcp");
N = 200;

server = tcp.createServer(function (connection) {
  function write (j) {
    if (j >= N) {
      connection.close();
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

client = tcp.createConnection(PORT);
client.setEncoding("ascii");
client.addListener("data", function (d) {
    print(d);
    recv += d;
});

setTimeout(function () {
  chars_recved = recv.length;
  puts("pause at: " + chars_recved);
  assert.equal(true, chars_recved > 1);
  client.pause();
  setTimeout(function () {
    puts("resume at: " + chars_recved);
    assert.equal(chars_recved, recv.length);
    client.resume();

    setTimeout(function () {
      chars_recved = recv.length;
      puts("pause at: " + chars_recved);
      client.pause();

      setTimeout(function () {
        puts("resume at: " + chars_recved);
        assert.equal(chars_recved, recv.length);
        client.resume();

      }, 500);

    }, 500);

  }, 500);

}, 500);

client.addListener("end", function () {
  server.close();
  client.close();
});

process.addListener("exit", function () {
  assert.equal(N, recv.length);
  debug("Exit");
});
