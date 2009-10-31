process.mixin(require("./common"));
http = require("http");
PORT = 8888;

var responses_sent = 0;
var responses_recvd = 0;
var body0 = "";
var body1 = "";

http.createServer(function (req, res) {
  if (responses_sent == 0) {
    assertEquals("GET", req.method);
    assertEquals("/hello", req.uri.path);

    p(req.headers);
    assertTrue("accept" in req.headers);
    assertEquals("*/*", req.headers["accept"]);

    assertTrue("foo" in req.headers);
    assertEquals("bar", req.headers["foo"]);
  }

  if (responses_sent == 1) {
    assertEquals("POST", req.method);
    assertEquals("/world", req.uri.path);
    this.close();
  }

  req.addListener("complete", function () {
    res.sendHeader(200, {"Content-Type": "text/plain"});
    res.sendBody("The path was " + req.uri.path);
    res.finish();
    responses_sent += 1;
  });

  //assertEquals("127.0.0.1", res.connection.remoteAddress);
}).listen(PORT);

var client = http.createClient(PORT);
var req = client.get("/hello", {"Accept": "*/*", "Foo": "bar"});
req.finish(function (res) {
  assertEquals(200, res.statusCode);
  responses_recvd += 1;
  res.setBodyEncoding("ascii");
  res.addListener("body", function (chunk) { body0 += chunk; });
  debug("Got /hello response");
});

setTimeout(function () {
  req = client.post("/world");
  req.finish(function (res) {
    assertEquals(200, res.statusCode);
    responses_recvd += 1;
    res.setBodyEncoding("utf8");
    res.addListener("body", function (chunk) { body1 += chunk; });
    debug("Got /world response");
  });
}, 1);

process.addListener("exit", function () {
  debug("responses_recvd: " + responses_recvd);
  assertEquals(2, responses_recvd);

  debug("responses_sent: " + responses_sent);
  assertEquals(2, responses_sent);

  assertEquals("The path was /hello", body0);
  assertEquals("The path was /world", body1);
});

