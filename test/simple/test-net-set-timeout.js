require("../common");
var sys = require('sys'),
   http = require('http');

server = http.createServer(function (req, res) {
  sys.puts('got request. setting 1 second timeout');
  req.connection.setTimeout(500);

  req.connection.addListener('timeout', function(){
    sys.debug("TIMEOUT");

    var body="timeout\n";
    res.writeHead(200, {
      'Content-Type': 'text/plain',
      'Content-Length': body.length,
      'Connection':'close'
    });
    res.end(body);
    req.connection.end();
    server.close();
  });
});
server.listen(8000);


server.addListener('listening', function () {
  sys.puts('Server running at http://127.0.0.1:8000/');

  errorTimer =setTimeout(function () {
    throw new Error('Timeout was not sucessful');
  }, 2000);

  http.cat('http://localhost:8000/', 'utf8', function (err, content) {
    clearTimeout(errorTimer);
    if (err) throw err;
    sys.puts('HTTP REQUEST COMPLETE (this is good)');
    sys.puts(content);
  });
});
