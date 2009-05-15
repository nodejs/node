new node.http.Server(function (msg) {
  var commands = msg.path.split("/");
  var body = "";
  var command = commands[1];
  var arg = commands[2];
  var status = 200;

  //p(msg.headers);

  if (command == "bytes") {
    var n = parseInt(arg, 10)
    if (n <= 0)
      throw "bytes called with n <= 0" 
    for (var i = 0; i < n; i++) {
      body += "C"
    }

  } else if (command == "quit") {
    msg.connection.server.close();
    body = "quitting";

  } else if (command == "fixed") {
    body = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC";

  } else {
    status = 404;
    body = "not found\n";
  }

  var content_length = body.length.toString();

  msg.sendHeader( status 
                , [ ["Content-Type", "text/plain"]
                  , ["Content-Length", content_length]
                  ]
                );
  msg.sendBody(body);
          
  msg.finish();
}).listen(8000);
