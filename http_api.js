function encode(data) { 
  var chunk = data.toString();
  return chunk.length.toString(16) + "\r\n" + chunk + "\r\n";
}

var port = 8000;
var server = new HTTP.Server("localhost", port);

server.onRequest = function (request) {

  // onBody sends null on the last chunk.
  request.onBody = function (chunk) {
    if(chunk) { 
      this.respond(encode(chunk));
    } else {
      this.respond(encode("\n"));
      this.respond("0\r\n\r\n");
      this.respond(null); // signals end-of-request
    }
  }
  request.respond("HTTP/1.0 200 OK\r\n");
  request.respond("Content-Type: text/plain\r\n");
  request.respond("Transfer-Encoding: chunked\r\n");
  request.respond("\r\n");
};


log("Running at http://localhost:" + port + "/");


/*
server.close();
*/
