process.mixin(require("./common"));
tcp = require("tcp");
http = require("http");

var port = 7333;

var body = "hello world\n";
var server_response = "";
var client_got_eof = false;

var server = http.createServer(function (req, res) {
  res.writeHeader(200, {"Content-Type": "text/plain"});
  res.write(body);
  res.close();
})
server.listen(port);

var c = tcp.createConnection(port);

c.setEncoding("utf8");

c.addListener("connect", function () {
  c.write( "GET / HTTP/1.0\r\n\r\n" );
});

c.addListener("data", function (chunk) {
  puts(chunk);
  server_response += chunk;
});

c.addListener("end", function () {
  client_got_eof = true;
  c.close();
  server.close();
});

process.addListener("exit", function () {
  var m = server_response.split("\r\n\r\n");
  assert.equal(m[1], body);
  assert.equal(true, client_got_eof);
});
