require("../common.js");
http = require("/http.js");

puts("hello world");

var body = "exports.A = function() { return 'A';}";
var server = http.createServer(function (req, res) {
    puts("req?");
  res.sendHeader(200, {
    "Content-Length": body.length,
    "Content-Type": "text/plain"
  });
  res.sendBody(body);
  res.finish();
});
server.listen(PORT);

var errors = 0;
var successes = 0;

var promise = process.cat("http://localhost:"+PORT, "utf8");

promise.addCallback(function (content) {
  assert.equal(body, content);
  server.close();
  successes += 1;
});

promise.addErrback(function () {
  errors += 1;
});

var dirname = process.path.dirname(__filename);
var fixtures = process.path.join(dirname, "fixtures");
var x = process.path.join(fixtures, "x.txt");

promise = process.cat(x, "utf8");

promise.addCallback(function (content) {
  assert.equal("xyz", content.replace(/[\r\n]/, ''));
  successes += 1;
});

promise.addErrback(function () {
  errors += 1;
});

process.addListener("exit", function () {
  assert.equal(2, successes);
  assert.equal(0, errors);
});
