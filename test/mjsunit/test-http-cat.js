process.mixin(require("./common"));
http = require("http");
PORT = 8888;

var body = "exports.A = function() { return 'A';}";
var server = http.createServer(function (req, res) {
  puts("got request");
  res.sendHeader(200, [
    ["Content-Length", body.length],
    ["Content-Type", "text/plain"]
  ]);
  res.write(body);
  res.close();
});
server.listen(PORT);

var got_good_server_content = false;
var bad_server_got_error = false;

http.cat("http://localhost:"+PORT+"/", "utf8", function (err, content) {
  if (err) {
    throw err;
  } else {
    puts("got response");
    got_good_server_content = true;
    assert.equal(body, content);
    server.close();
  }
});

http.cat("http://localhost:12312/", "utf8", function (err, content) {
  if (err) { 
    puts("got error (this should happen)");
    bad_server_got_error = true;
  } 
});

process.addListener("exit", function () {
  puts("exit");
  assert.equal(true, got_good_server_content);
  assert.equal(true, bad_server_got_error);
});
