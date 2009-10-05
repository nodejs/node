var s = node.http.createServer(function (req, res) {
  var body = "exports.A = function() { return 'A';}";
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.sendBody(body);
  res.finish();
});
s.listen(8000);

node.mixin(require("../common.js"));
var a = require("http://localhost:8000/")

assertInstanceof(a.A, Function);
assertEquals("A", a.A());
s.close();
