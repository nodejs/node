require('../common');

assert = require("assert");
http = require("http");
sys = require("sys");


body = "hello world\n";

server = http.createServer(function (req, res) {
  error('req: ' + req.method);
  res.writeHead(200, {"Content-Length": body.length});
  res.end();
  server.close();
});
server.listen(PORT);

var gotEnd = false;

server.addListener('listening', function () {
  var client = http.createClient(PORT);
  var request = client.request("HEAD", "/");
  request.addListener('response', function (response) {
    error('response start');
    response.addListener("end", function () {
      error('response end');
      gotEnd = true;
    });
  });
  request.end();
});

process.addListener('exit', function () {
  assert.ok(gotEnd);
});
