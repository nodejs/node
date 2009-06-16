include("mjsunit.js");
PORT = 8888;

var server = new node.http.Server(function (req, res) {
  res.sendHeader(200, [["content-type", "text/plain"]]);
  if (req.uri.path == "/1")
    res.sendBody("hello world 1\n");
  else
    res.sendBody("hello world 2\n");
  res.finish();
})
server.listen(PORT);

var client = new node.http.Client(PORT);

var body1 = "";
var body2 = "";

client.get("/1").finish(function (res1) {
  res1.setBodyEncoding("utf8");

  res1.onBody = function (chunk) { body1 += chunk; };

  res1.onBodyComplete = function () {
    client.get("/2").finish(function (res2) {
      res2.setBodyEncoding("utf8");
      res2.onBody = function (chunk) { body2 += chunk; };
      res2.onBodyComplete = function () {
        server.close();
      };
    });
  };
});

function onExit () {
  assertEquals("hello world 1\n", body1);
  assertEquals("hello world 2\n", body2);
}
