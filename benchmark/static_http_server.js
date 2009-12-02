var path = require("path");
libDir = path.join(path.dirname(__filename), "../lib");
require.paths.unshift(libDir);
http = require("http");
var concurrency = 30;
var port = 8000;
var n = 700;
var bytes = 1024*5;

var requests = 0;
var responses = 0;

var body = "";
for (var i = 0; i < bytes; i++) {
  body += "C";
}

var server = http.createServer(function (req, res) {
  res.sendHeader(200, {
    "Content-Type": "text/plain",
    "Content-Length": body.length
  });
  res.sendBody(body);
  res.finish();
})
server.listen(port);

function responseListener (res) {
  res.addListener("complete", function () {
    if (requests < n) {
      res.client.request("/").finish(responseListener);
      requests++;
    }

    if (++responses == n) {
      server.close();
    }
  });
}

for (var i = 0; i < concurrency; i++) {
  var client = http.createClient(port);
  client.id = i;
  client.request("/").finish(responseListener);
  requests++;
}
