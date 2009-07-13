var concurrency = 30;
var nrequests = 700;
var port = 8000;
var completed_requests = 0;
var bytes = 1024*5;

var body = "";
for (var i = 0; i < bytes; i++) {
  body += "C";
}

var server = node.http.createServer(function (req, res) {
  res.sendHeader(200, [
    ["Content-Type", "text/plain"],
    ["Content-Length", body.length]
  ]);
  res.sendBody(body);
  res.finish();
})
server.listen(port);

function responseListener (res) {
  res.addListener("complete", function () {
    //puts("response " + completed_requests + " from client " + res.client.id);
    if (completed_requests++ < nrequests) {
      res.client.get("/").finish(responseListener);
    } else {
      server.close();
    }
  });
}

function onLoad () {
  for (var i = 0; i < concurrency; i++) {
    var client = node.http.createClient(port);
    client.id = i;
    client.get("/").finish(responseListener);
  }
}
