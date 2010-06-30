path = require("path");
Buffer = require("buffer").Buffer;

port = parseInt(process.env.PORT || 8000);

var old = (process.argv[2] == 'old');

console.log('pid ' + process.pid);

http = require(old ? "http_old" : 'http');
if (old) console.log('old version');

fixed = ""
for (var i = 0; i < 20*1024; i++) {
  fixed += "C";
}

stored = {};
storedBuffer = {};

http.createServer(function (req, res) {
  var commands = req.url.split("/");
  var command = commands[1];
  var body = "";
  var arg = commands[2];
  var status = 200;

  if (command == "bytes") {
    var n = parseInt(arg, 10)
    if (n <= 0)
      throw "bytes called with n <= 0"
    if (stored[n] === undefined) {
      console.log("create stored[n]");
      stored[n] = "";
      for (var i = 0; i < n; i++) {
        stored[n] += "C"
      }
    }
    body = stored[n];

  } else if (command == "buffer") {
    var n = parseInt(arg, 10)
    if (n <= 0) throw new Error("bytes called with n <= 0");
    if (storedBuffer[n] === undefined) {
      console.log("create storedBuffer[n]");
      storedBuffer[n] = new Buffer(n);
      for (var i = 0; i < n; i++) {
        storedBuffer[n][i] = "C".charCodeAt(0);
      }
    }
    body = storedBuffer[n];

  } else if (command == "quit") {
    res.connection.server.close();
    body = "quitting";

  } else if (command == "fixed") {
    body = fixed;

  } else {
    status = 404;
    body = "not found\n";
  }

  var content_length = body.length.toString();

  res.writeHead( status
                , { "Content-Type": "text/plain"
                  , "Content-Length": content_length
                  }
                );
  if (old) {
    res.write(body, 'ascii');
    res.close();
  } else {
    res.end(body, 'ascii');
  }
}).listen(port);

console.log('Listening at http://127.0.0.1:'+port+'/');
