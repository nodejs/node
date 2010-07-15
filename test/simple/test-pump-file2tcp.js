common = require("../common");
assert = common.assert
net = require("net");
fs = require("fs");
sys = require("sys");
path = require("path");
fn = path.join(common.fixturesDir, 'elipses.txt');

expected = fs.readFileSync(fn, 'utf8');

server = net.createServer(function (stream) {
  common.error('pump!');
  sys.pump(fs.createReadStream(fn), stream, function () {
    common.error('server stream close');
    common.error('server close');
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
  });
});

var buffer = '';
count = 0;

server.addListener('listening', function () {
});

process.addListener('exit', function () {
  assert.equal(expected, buffer);
});
