new node.http.Server(function (msg) {
  setTimeout(function () {
    msg.sendHeader(200, [["Content-Type", "text/plain"]]);
    msg.sendBody("Hello World");
    msg.finish();
  }, 1000);
}).listen(8000, "localhost");
