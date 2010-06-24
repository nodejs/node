require("../common");
var Buffer = require('buffer').Buffer;
var dgram = require("dgram");

var tests_run = 0;


function pingPongTest (port, host) {
  var callbacks = 0;
  var N = 500;
  var count = 0;
  var sent_final_ping = false;

  var server = dgram.createSocket(function (msg, rinfo) {
    console.log("connection: " + rinfo.address + ":"+ rinfo.port);

    console.log("server got: " + msg);

    if (/PING/.exec(msg)) {
      var buf = new Buffer(4);
      buf.write('PONG');
      server.send(rinfo.port, rinfo.address, buf, 0, buf.length, function (err, sent) {
        callbacks++;
      });
    }

  });

  server.addListener("error", function (e) {
    throw e;
  });

  server.bind(port, host);

  server.addListener("listening", function () {
    console.log("server listening on " + port + " " + host);

    var buf = new Buffer(4);
    buf.write('PING');

    var client = dgram.createSocket();

    client.addListener("message", function (msg, rinfo) {
      console.log("client got: " + msg);
      assert.equal("PONG", msg.toString('ascii'));

      count += 1;

      if (count < N) {
        client.send(port, host, buf, 0, buf.length);
      } else {
        sent_final_ping = true;
        client.send(port, host, buf, 0, buf.length);
        process.nextTick(function() {
          client.close();
        });
      }
    });

    client.addListener("close", function () {
      console.log('client.close');
      assert.equal(N, count);
      tests_run += 1;
      server.close();
      assert.equal(N-1, callbacks);
    });

    client.addListener("error", function (e) {
      throw e;
    });

    client.send(port, host, buf, 0, buf.length);
    count += 1;
  });

}

/* All are run at once, so run on different ports */
pingPongTest(20989, "localhost");
pingPongTest(20990, "localhost");
pingPongTest(20988);
pingPongTest(20997, "::1");
//pingPongTest("/tmp/pingpong.sock");

process.addListener("exit", function () {
  assert.equal(4, tests_run);
  console.log('done');
});
