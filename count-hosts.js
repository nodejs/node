
function Process(request) {

  //
  //  request.headers      => { "Content-Length": "123" }
  //  request.http_version => "1.1"
  //
  //log("Processing " + request.path + ". method: " + request.method);

  // sends null on the last chunk.
  request.onBody = function (chunk) {
    if(chunk) {
      //log( "got chunk length: " + chunk.length.toString() );
      this.respond(chunk.length.toString(16) + "\r\n" + chunk + "\r\n");
    } else {
      this.respond("0\r\n\r\n");
      this.respond(null);
    }
  }

  request.respond("HTTP/1.0 200 OK\r\n");
  request.respond("Content-Type: text-plain\r\n");
  request.respond("Transfer-Encoding: chunked\r\n");
  request.respond("\r\n");
  request.respond("0\r\n\r\n");
  //request.respond("Content-Length: 6\r\n\r\nhello\n");  
  //
}

