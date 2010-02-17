process.mixin(require("./common"));
tcp = require("tcp");
http = require("http");
url = require("url");

// Make sure no exceptions are thrown when receiving malformed HTTP
// requests.
port = 9999;

nrequests_completed = 0;
nrequests_expected = 1;

var s = http.createServer(function (req, res) {
  puts("req: " + JSON.stringify(url.parse(req.url)));

  res.sendHeader(200, {"Content-Type": "text/plain"});
  res.write("Hello World");
  res.close();

  if (++nrequests_completed == nrequests_expected) s.close();
});
s.listen(port);

var c = tcp.createConnection(port);
c.addListener("connect", function () {
  c.write("GET /hello?foo=%99bar HTTP/1.1\r\n\r\n");
  c.close();
});

//  TODO add more!

process.addListener("exit", function () {
  assert.equal(nrequests_expected, nrequests_completed);
});
