common = require("../common");
assert = common.assert
net = require("net");
fs = require("fs");
sys = require("sys");
path = require("path");
fn = path.join(common.fixturesDir, 'does_not_exist.txt');

var got_error = false;
var conn_closed = false;

server = net.createServer(function (stream) {
  common.error('pump!');
  sys.pump(fs.createReadStream(fn), stream, function (err) {
    common.error("sys.pump's callback fired");
    if (err) {
      got_error = true;
    } else {
      common.debug("sys.pump's callback fired with no error");
      common.debug("this shouldn't happen as the file doesn't exist...");
      assert.equal(true, false);
    }
    server.close();
  });
});

server.listen(common.PORT, function () {
  conn = net.createConnection(common.PORT);
  conn.setEncoding('utf8');
  conn.addListener("data", function (chunk) {
    common.error('recv data! nchars = ' + chunk.length);
    buffer += chunk;
  });

  conn.addListener("end", function () {
    conn.end();
  });

  conn.addListener("close", function () {
    common.error('client connection close');
    conn_closed = true;
  });
});

var buffer = '';
count = 0;

process.addListener('exit', function () {
  assert.equal(true, got_error);
  assert.equal(true, conn_closed);
  console.log("exiting");
});
