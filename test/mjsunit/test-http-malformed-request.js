process.mixin(require("./common"));
tcp = require("tcp");
http = require("http");

// Make sure no exceptions are thrown when receiving malformed HTTP
// requests.
port = 9999;

nrequests_completed = 0;
nrequests_expected = 2;

var server = http.createServer(function (req, res) {
  puts("req: " + JSON.stringify(req.uri));

  res.sendHeader(200, {"Content-Type": "text/plain"});
  res.sendBody("Hello World");
  res.finish();

  if (++nrequests_completed == nrequests_expected) server.close();

  puts("nrequests_completed: " + nrequests_completed);
});
server.listen(port);

tcp.createConnection(port).addListener("connect", function () {
  this.send("GET /hello?foo=%99bar HTTP/1.1\r\nConnection: close\r\n\r\n");
  this.close();
});

tcp.createConnection(port).addListener("connect", function () {
  this.send("GET /with_\"stupid\"_quotes?in_the=\"uri\" HTTP/1.1\r\nConnection: close\r\n\r\n");
  this.close();
});

process.addListener("exit", function () {
  assertEquals(nrequests_expected, nrequests_completed);
});
