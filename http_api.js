function encode(data) { 
  var chunk = data.toString();
  return chunk.length.toString(16) + "\r\n" + chunk + "\r\n";
}

var port = 8000;
var server = new HTTPServer("localhost", port);

server.onrequest = function (request) {
  log("path: " + request.path);
  log("query string: " + request.query_string);

  request.respond("HTTP/1.1 200 OK\r\n");
  request.respond("Content-Length: 0\r\n");
  request.respond("\r\n");
  request.respond(null);
};


log("Running at http://localhost:" + port + "/");


/*
server.close();
*/
