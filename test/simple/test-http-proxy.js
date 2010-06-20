require("../common");
http = require("http");
url = require("url");

var PROXY_PORT = PORT;
var BACKEND_PORT = PORT+1;

var backend = http.createServer(function (req, res) {
  debug("backend request");
  res.writeHead(200, {"content-type": "text/plain"});
  res.write("hello world\n");
  res.end();
});

var proxy_client = http.createClient(BACKEND_PORT);
var proxy = http.createServer(function (req, res) {
  debug("proxy req headers: " + JSON.stringify(req.headers));
  var proxy_req = proxy_client.request(url.parse(req.url).pathname);
  proxy_req.end();
  proxy_req.addListener('response', function(proxy_res) {
    res.writeHead(proxy_res.statusCode, proxy_res.headers);
    proxy_res.addListener("data", function(chunk) {
      res.write(chunk);
    });
    proxy_res.addListener("end", function() {
      res.end();
      debug("proxy res");
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
  debug("client req")
  req.addListener('response', function (res) {
    debug("got res");
    assert.equal(200, res.statusCode);
    res.setBodyEncoding("utf8");
    res.addListener('data', function (chunk) { body += chunk; });
    res.addListener('end', function () {
      proxy.close();
      backend.close();
       debug("closed both");
    });
  });
  req.end();
}

debug("listen proxy")
proxy.listen(PROXY_PORT, startReq);

debug("listen backend")
backend.listen(BACKEND_PORT, startReq);

process.addListener("exit", function () {
  assert.equal(body, "hello world\n");
});
