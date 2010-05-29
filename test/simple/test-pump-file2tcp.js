require("../common");
net = require("net");
fs = require("fs");
sys = require("sys");
path = require("path");
fn = path.join(fixturesDir, 'elipses.txt');

expected = fs.readFileSync(fn, 'utf8');

server = net.createServer(function (stream) {
  error('pump!');
  sys.pump(fs.createReadStream(fn), stream, function () {
    error('server stream close');
    error('server close');
    server.close();
  });
});

server.listen(PORT, function () {
  conn = net.createConnection(PORT);
  conn.setEncoding('utf8');
  conn.addListener("data", function (chunk) {
    error('recv data! nchars = ' + chunk.length);
    buffer += chunk;
  });

  conn.addListener("end", function () {
    conn.end();
  });
  conn.addListener("close", function () {
    error('client connection close');
  });
});

var buffer = '';
count = 0;

server.addListener('listening', function () {
});

process.addListener('exit', function () {
  assert.equal(expected, buffer);
});
