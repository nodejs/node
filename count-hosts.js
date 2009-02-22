function Process(request) {
  log("Processing " + request.path + ". method: " + request.method);

  // sends null on the last chunk.
  request.onBody = function (chunk) {
    log("body chunk: '" + chunk + "'");
  }

  request.respond("HTTP/1.0 200 OK\r\n")
  request.respond("Content-Type: text-plain\r\nContent-Length: 6\r\n\r\nhello\n");  
  request.respond(null); // eof
}
