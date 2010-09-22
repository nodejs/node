common = require("../common");
assert = common.assert

assert = require("assert");
http = require("http");
sys = require("sys");

body = "hello world\n";
headers = {'connection':'keep-alive'}

server = http.createServer(function (req, res) {
  res.writeHead(200, {"Content-Length": body.length, "Connection":"close"});
  res.write(body);
  res.end();
});

connectCount = 0;

server.listen(common.PORT, function () {
  var client = http.createClient(common.PORT);

  client.addListener("connect", function () {
    common.error("CONNECTED")
    connectCount++;
  })

  var request = client.request("GET", "/", headers);
  request.end();
  request.addListener('response', function (response) {
    common.error('response start');


    response.addListener("end", function () {
      common.error('response end');
      var req = client.request("GET", "/", headers);
      req.addListener('response', function (response) {
        response.addListener("end", function () {
          client.end();
          server.close();
        })
      })
      req.end();
    });
  });
});

process.addListener('exit', function () {
  assert.equal(2, connectCount);
});
