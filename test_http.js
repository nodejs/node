new node.http.Server(function (req, res) {
  setTimeout(function () {
    res.sendHeader(200, [["Content-Type", "text/plain"]]);
    res.sendBody("Hello World");
    res.finish();
  }, 1000);
}).listen(8000, "localhost");
