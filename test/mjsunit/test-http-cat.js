include("mjsunit.js");
PORT = 8888;

var body = "exports.A = function() { return 'A';}";
var server = new node.http.Server(function (req, res) {
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

function onLoad() {
  node.http.cat("http://localhost:"+PORT, "utf8", function(status, content) {
    assertEquals(body, content);
    assertEquals(0, status)
    server.close()
  })

  node.http.cat("http://localhost:"+PORT+1, "utf8", function(status, content) {
    assertEquals(-1, status)
    assertEquals(nil, content)
  })
}
