function encode(i) { 
  var string = i.toString();
  var r = string.length.toString(16) + "\r\n" + string + "\r\n";
  return r;
}

function Process(request) {

  //
  //  request.headers      => { "Content-Length": "123" }
  //
//  log("Processing " + request.path + ". method: " + request.method);
//  log("version " + request.http_version);
//  log("query_string " + request.query_string);
//  log("fragment " + request.fragment);
//  log("uri " + request.uri);
//  log("headers: " + request.headers.toString());
  //for(var f in request.headers) {
    //log(f + ": " + request.headers[f]);
  //}
  var s  =  "";

  // sends null on the last chunk.
  request.onBody = function (chunk) {
    if(chunk) {
      //log( "got chunk length: " + chunk.length.toString() );
      //this.respond(chunk.length.toString(16) + "\r\n" + evalchunk + "\r\n");
      s +=  chunk;
    } else {
      this.respond(encode("hello world\n"));
      var output = eval(s);
      if(output) {
        this.respond(encode(output));
      }
      this.respond(encode("\n"));
      this.respond("0\r\n\r\n");
      this.respond(null);
    }
  }

  request.respond("HTTP/1.0 200 OK\r\n");
  request.respond("Content-Type: text-plain\r\n");
  request.respond("Transfer-Encoding: chunked\r\n");
  request.respond("\r\n");
}

