include("mjsunit.js");

var port = 8222;

var request_number = 0;
var requests_sent = 0;
var server_response = "";
var client_got_eof = false;

node.http.createServer(function (req, res) {
  res.id = request_number;
  req.id = request_number++;

  if (req.id == 0) {
    assertEquals("GET", req.method);
    assertEquals("/hello", req.uri.path);
  }

  if (req.id == 1) {
    assertEquals("POST", req.method);
    assertEquals("/quit", req.uri.path);
    this.close();
    //puts("server closed");
  }

  setTimeout(function () {
    res.sendHeader(200, {"Content-Type": "text/plain"});
    res.sendBody(req.uri.path);
    res.finish();
  }, 1);

}).listen(port);

var c = node.tcp.createConnection(port);

c.setEncoding("utf8");

c.addListener("connect", function () {
  c.send( "GET /hello HTTP/1.1\r\n\r\n" );
  requests_sent += 1;
});

c.addListener("receive", function (chunk) {
  server_response += chunk;

  if (requests_sent == 1) {
    c.send("POST /quit HTTP/1.1\r\n\r\n");
    c.close();
    assertEquals(c.readyState, "readOnly");
    requests_sent += 1;
  }
});

c.addListener("eof", function () {
  client_got_eof = true;
});

c.addListener("close", function () {
  assertEquals(c.readyState, "closed");
});

function onExit () {
  assertEquals(2, request_number);
  assertEquals(2, requests_sent);

  var hello = new RegExp("/hello");
  assertTrue(hello.exec(server_response) != null);

  var quit = new RegExp("/quit");
  assertTrue(quit.exec(server_response) != null);

  assertTrue(client_got_eof);
}
