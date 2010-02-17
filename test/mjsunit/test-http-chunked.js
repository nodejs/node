process.mixin(require("./common"));
var http = require("http");
var PORT = 8888;

var UTF8_STRING = "Il était tué";

var server = http.createServer(function(req, res) {
  res.sendHeader(200, {"Content-Type": "text/plain; charset=utf8"});
  res.write(UTF8_STRING, 'utf8');
  res.close();
});
server.listen(PORT);

http.cat("http://localhost:"+PORT+"/", "utf8")
  .addCallback(function (data) {
    assert.equal(UTF8_STRING, data);
    server.close();
  })
  .addErrback(function() {
    assert.ok(false, 'http.cat should succeed in < 1000ms');
  })
  .timeout(1000);
