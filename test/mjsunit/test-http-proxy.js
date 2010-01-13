process.mixin(require("./common"));
http = require("http");
url = require("url");

var PROXY_PORT = 8869;
var BACKEND_PORT = 8870;

var backend = http.createServer(function (req, res) {
  // debug("backend");
  res.sendHeader(200, {"content-type": "text/plain"});
  res.sendBody("hello world\n");
  res.finish();
});
// debug("listen backend")
backend.listen(BACKEND_PORT);

var proxy_client = http.createClient(BACKEND_PORT);
var proxy = http.createServer(function (req, res) {
  debug("proxy req headers: " + JSON.stringify(req.headers));
  var proxy_req = proxy_client.request(url.parse(req.url).pathname);
  proxy_req.finish(function(proxy_res) {
    res.sendHeader(proxy_res.statusCode, proxy_res.headers);
    proxy_res.addListener("body", function(chunk) {
      res.sendBody(chunk);
    });
    proxy_res.addListener("complete", function() {
      res.finish();
      // debug("proxy res");
    });
  });
});
// debug("listen proxy")
proxy.listen(PROXY_PORT);

var body = "";

var client = http.createClient(PROXY_PORT);
var req = client.request("/test");
// debug("client req")
req.finish(function (res) {
  // debug("got res");
  assert.equal(200, res.statusCode);
  res.setBodyEncoding("utf8");
  res.addListener("body", function (chunk) { body += chunk; });
  res.addListener("complete", function () {
    proxy.close();
    backend.close();
    // debug("closed both");
  });
});

process.addListener("exit", function () {
  assert.equal(body, "hello world\n");
});
