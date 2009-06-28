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

var errors = 0;
var successes = 0;

function onLoad () {
  var promise = node.cat("http://localhost:"+PORT, "utf8");
  
  promise.addCallback(function (content) {
    assertEquals(body, content);
    server.close()
    successes += 1;
  });

  promise.addErrback(function () {
    errors += 1;
  });
  
  var dirname = node.path.dirname(__filename);
  var fixtures = node.path.join(dirname, "fixtures");
  var x = node.path.join(fixtures, "x.txt");

  promise = node.cat(x, "utf8");
  
  promise.addCallback(function (content) {
    assertEquals("xyz", content.replace(/[\r\n]/, ''))
    successes += 1;
  });

  promise.addErrback(function () {
    errors += 1;
  });
}

function onExit () {
  assertEquals(2, successes);
  assertEquals(0, errors);
}
