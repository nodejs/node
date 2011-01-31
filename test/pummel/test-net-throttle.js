var common = require('../common');
var assert = require('assert');
var net = require('net');

var N = 160 * 1024;
var chars_recved = 0;
var npauses = 0;

console.log('build big string');
var body = '';
for (var i = 0; i < N; i++) {
  body += 'C';
}

console.log('start server on port ' + common.PORT);

var server = net.createServer(function(connection) {
  connection.addListener('connect', function() {
    assert.equal(false, connection.write(body));
    console.log('bufferSize: ' + connection.bufferSize);
    assert.ok(0 <= connection.bufferSize &&
              connection.bufferSize <= N);
    connection.end();
  });
});

server.listen(common.PORT, function() {
  var paused = false;
  var client = net.createConnection(common.PORT);
  client.setEncoding('ascii');
  client.addListener('data', function(d) {
    chars_recved += d.length;
    console.log('got ' + chars_recved);
    if (!paused) {
      client.pause();
      npauses += 1;
      paused = true;
      console.log('pause');
      var x = chars_recved;
      setTimeout(function() {
        assert.equal(chars_recved, x);
        client.resume();
        console.log('resume');
        paused = false;
      }, 100);
    }
  });

  client.addListener('end', function() {
    server.close();
    client.end();
  });
});



process.addListener('exit', function() {
  assert.equal(N, chars_recved);
  assert.equal(true, npauses > 2);
});
