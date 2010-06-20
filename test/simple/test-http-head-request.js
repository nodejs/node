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

var gotEnd = false;

server.listen(PORT, function () {
  var client = http.createClient(PORT);
  var request = client.request("HEAD", "/");
  request.end();
  request.addListener('response', function (response) {
    error('response start');
    response.addListener("end", function () {
      error('response end');
      gotEnd = true;
    });
  });
});

process.addListener('exit', function () {
  assert.ok(gotEnd);
});
