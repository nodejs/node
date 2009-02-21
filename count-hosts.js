function Process(request) {
  log("Processing " + request.path + ". method: " + request.method);

  /*
  request.onBody = function (chunk) {
    log("body chunk: " + chunk);
  }
  request.respond("HTTP/1.1 200 OK\r\nContent-Type: text-plain\r\n\r\nhello");  
  request.response_complete 
  */
}
