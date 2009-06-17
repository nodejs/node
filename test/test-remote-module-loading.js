new node.http.Server(function (req, res) {
  var body = "exports.A = function() { return 'A';}";
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
}).listen(8000);

include("mjsunit.js");
var a = require("http://localhost:8000/")

function onLoad() {
  assertInstanceof(a.A, Function);
  assertEquals("A", a.A());
  
}

// TODO: Add tests for remote require using a timeout