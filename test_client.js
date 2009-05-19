var c = new node.http.Client(8000, "localhost")

var req = c.get("/hello/world", [["Accept", "*/*"]]);
req.finish(function (res) {
  puts("got response: " + res.status_code.toString());

  res.onBody = function (chunk) {
    puts("got response body <" + chunk.encodeUtf8() + ">");
    return true;
  };

  res.onBodyComplete = function () {
    puts("response complete!");
    return true;
  };
});
