include("mjsunit.js");
PORT = 8888;

var body = "exports.A = function() { return 'A';}";
new node.http.Server(function (req, res) {
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
}).listen(PORT);

function onLoad() {
  try {
    node.http.cat("http://localhost:"+PORT, "utf8", function(status, content) {
      assertEquals(body, content);
      // TODO: Test the status code
    })
  } catch(e) {
    assertUnreachable(e)
  }
}
