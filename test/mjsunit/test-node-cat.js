include("mjsunit.js");
PORT = 8888;

var body = "exports.A = function() { return 'A';}";
var server = node.http.createServer(function (req, res) {
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

function onLoad() {
  node.cat("http://localhost:"+PORT, "utf8", function(status, content) {
    assertEquals(body, content);
    assertEquals(0, status)
    server.close()
  })
  
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");
  node.cat(x, "utf8", function(status, content) {
    assertEquals(0, status)
    assertEquals("xyz", content.replace(/[\r\n]/, ''))
  })
}
