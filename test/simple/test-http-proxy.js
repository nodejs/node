common = require("../common");
assert = common.assert
http = require("http");
url = require("url");

var PROXY_PORT = common.PORT;
var BACKEND_PORT = common.PORT+1;

var backend = http.createServer(function (req, res) {
  common.debug("backend request");
  res.writeHead(200, {"content-type": "text/plain"});
  res.write("hello world\n");
  res.end();
});

var proxy_client = http.createClient(BACKEND_PORT);
var proxy = http.createServer(function (req, res) {
  common.debug("proxy req headers: " + JSON.stringify(req.headers));
  var proxy_req = proxy_client.request(url.parse(req.url).pathname);
  proxy_req.end();
  proxy_req.addListener('response', function(proxy_res) {
    res.writeHead(proxy_res.statusCode, proxy_res.headers);
    proxy_res.addListener("data", function(chunk) {
      res.write(chunk);
    });
    proxy_res.addListener("end", function() {
      res.end();
      common.debug("proxy res");
    });
  });
});

var body = "";

nlistening = 0;
function startReq () {
  nlistening++;
  if (nlistening < 2) return;

  var client = http.createClient(PROXY_PORT);
  var req = client.request("/test");
  common.debug("client req")
  req.addListener('response', function (res) {
    common.debug("got res");
    assert.equal(200, res.statusCode);
    res.setBodyEncoding("utf8");
    res.addListener('data', function (chunk) { body += chunk; });
    res.addListener('end', function () {
      proxy.close();
      backend.close();
       common.debug("closed both");
    });
  });
  req.end();
}

common.debug("listen proxy")
proxy.listen(PROXY_PORT, startReq);

common.debug("listen backend")
backend.listen(BACKEND_PORT, startReq);

process.addListener("exit", function () {
  assert.equal(body, "hello world\n");
});
