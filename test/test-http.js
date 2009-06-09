include("mjsunit.js");
PORT = 8888;

var responses_sent = 0;
var responses_recvd = 0;
var body0 = "";
var body1 = "";

function onLoad () {
  new node.http.Server(function (req, res) {
    if (responses_sent == 0) {
      assertEquals("GET", req.method);
      assertEquals("/hello", req.uri.path);
    }

    if (responses_sent == 1) {
      assertEquals("POST", req.method);
      assertEquals("/world", req.uri.path);
      this.close();
    }

    res.sendHeader(200, [["Content-Type", "text/plain"]]);
    res.sendBody("The path was " + req.uri.path);
    res.finish();
    responses_sent += 1;
  }).listen(PORT);

  var client = new node.http.Client(PORT);
  var req = client.get("/hello");
  req.finish(function (res) {
    assertEquals(200, res.statusCode);
    responses_recvd += 1;
    res.setBodyEncoding("utf8");
    res.onBody = function (chunk) { body0 += chunk; };
  });

  setTimeout(function () {
    req = client.post("/world");
    req.finish(function (res) {
      assertEquals(200, res.statusCode);
      responses_recvd += 1;
      res.setBodyEncoding("utf8");
      res.onBody = function (chunk) { body1 += chunk; };
    });
  }, 10);
}

function onExit () {
  assertEquals(2, responses_recvd);
  assertEquals(2, responses_sent);
  assertEquals("The path was /hello", body0);
  assertEquals("The path was /world", body1);
}

