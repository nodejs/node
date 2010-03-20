require("../common");
var http = require("http");

var UTF8_STRING = "Il était tué";

var server = http.createServer(function(req, res) {
  res.writeHead(200, {"Content-Type": "text/plain; charset=utf8"});
  res.write(UTF8_STRING, 'utf8');
  res.close();
});
server.listen(PORT);

http.cat("http://127.0.0.1:"+PORT+"/", "utf8", function (err, data) {
  if (err) throw err;
  assert.equal(UTF8_STRING, data);
  server.close();
})
