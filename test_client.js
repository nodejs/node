var c = new node.http.Client(8000, "localhost")

var req = c.get("/hello/world", [["Accept", "*/*"]]);
req.finish(function (res) {
  puts("response 1: " + res.status_code.toString());

  res.onBody = function (chunk) {
    puts("response 1 body <" + chunk.encodeUtf8() + ">");
    return true;
  };

  res.onBodyComplete = function () {
    puts("response 1 complete!");
    return true;
  };
});

/*
var req = c.get("/something/else", []);
req.finish(function (res) {
  puts("response 2: " + res.status_code.toString());

  res.onBody = function (chunk) {
    puts("response 2 body <" + chunk.encodeUtf8() + ">");
    return true;
  };

  res.onBodyComplete = function () {
    puts("response 2 complete!");
    return true;
  };
});
*/
