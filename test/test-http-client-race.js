include("mjsunit.js");
PORT = 8888;

var server = new node.http.Server(function (req, res) {
  res.sendHeader(200, [["content-type", "text/plain"]]);
  res.sendBody("hello world\n");
  res.finish();
})
server.listen(PORT);

var client = new node.http.Client(PORT);

var body1 = "";
var body2 = "";

client.get("/").finish(function (res1) {
  res1.setBodyEncoding("utf8");

  res1.onBody = function (chunk) { body1 += chunk; };

  res1.onBodyComplete = function () {
    client.get("/").finish(function (res2) {
      res2.setBodyEncoding("utf8");
      res2.onBody = function (chunk) { body2 += chunk; };
      res2.onBodyComplete = function () {
        server.close();
      };
    });
  };
});

function onExit () {
  assertEqual("hello world\n", body1);
  assertEqual("hello world\n", body2);
}
