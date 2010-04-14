require("../common");
http = require("http");
url = require("url");

var body1_s = "1111111111111111";
var body2_s = "22222";

var server = http.createServer(function (req, res) {
  var body = url.parse(req.url).pathname === "/1" ? body1_s : body2_s;
  res.writeHead(200, { "Content-Type": "text/plain"
                      , "Content-Length": body.length
                      });
  res.end(body);
});
server.listen(PORT);

var client = http.createClient(PORT);

var body1 = "";
var body2 = "";

var req1 = client.request("/1")
req1.addListener('response', function (res1) {
  res1.setBodyEncoding("utf8");

  res1.addListener('data', function (chunk) {
    body1 += chunk;
  });

  res1.addListener('end', function () {
    var req2 = client.request("/2");
    req2.addListener('response', function (res2) {
      res2.setBodyEncoding("utf8");
      res2.addListener('data', function (chunk) { body2 += chunk; });
      res2.addListener('end', function () { server.close(); });
    });
    req2.end();
  });
});
req1.end();

process.addListener("exit", function () {
  assert.equal(body1_s, body1);
  assert.equal(body2_s, body2);
});
