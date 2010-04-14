require("../common");
http = require("http");
url = require("url");

var responses_sent = 0;
var responses_recvd = 0;
var body0 = "";
var body1 = "";

http.createServer(function (req, res) {
  if (responses_sent == 0) {
    assert.equal("GET", req.method);
    assert.equal("/hello", url.parse(req.url).pathname);

    p(req.headers);
    assert.equal(true, "accept" in req.headers);
    assert.equal("*/*", req.headers["accept"]);

    assert.equal(true, "foo" in req.headers);
    assert.equal("bar", req.headers["foo"]);
  }

  if (responses_sent == 1) {
    assert.equal("POST", req.method);
    assert.equal("/world", url.parse(req.url).pathname);
    this.close();
  }

  req.addListener('end', function () {
    res.writeHead(200, {"Content-Type": "text/plain"});
    res.write("The path was " + url.parse(req.url).pathname);
    res.end();
    responses_sent += 1;
  });

  //assert.equal("127.0.0.1", res.connection.remoteAddress);
}).listen(PORT);

var client = http.createClient(PORT);
var req = client.request("/hello", {"Accept": "*/*", "Foo": "bar"});
req.addListener('response', function (res) {
  assert.equal(200, res.statusCode);
  responses_recvd += 1;
  res.setBodyEncoding("ascii");
  res.addListener('data', function (chunk) { body0 += chunk; });
  debug("Got /hello response");
});
req.end();

setTimeout(function () {
  req = client.request("POST", "/world");
  req.addListener('response',function (res) {
    assert.equal(200, res.statusCode);
    responses_recvd += 1;
    res.setBodyEncoding("utf8");
    res.addListener('data', function (chunk) { body1 += chunk; });
    debug("Got /world response");
  });
  req.end();
}, 1);

process.addListener("exit", function () {
  debug("responses_recvd: " + responses_recvd);
  assert.equal(2, responses_recvd);

  debug("responses_sent: " + responses_sent);
  assert.equal(2, responses_sent);

  assert.equal("The path was /hello", body0);
  assert.equal("The path was /world", body1);
});

