var http = require('http');

var concurrency = 30;
var port = 12346;
var n = 700;
var bytes = 1024*5;

var requests = 0;
var responses = 0;

var body = '';
for (var i = 0; i < bytes; i++) {
  body += 'C';
}

var server = http.createServer(function(req, res) {
  res.writeHead(200, {
    'Content-Type': 'text/plain',
    'Content-Length': body.length
  });
  res.end(body);
})

server.listen(port, function() {
  var agent = new http.Agent();
  agent.maxSockets = concurrency;

  for (var i = 0; i < n; i++) {
    var req = http.get({
      port:  port,
      path:  '/',
      agent: agent
    }, function(res) {
      res.resume();
      res.on('end', function() {
        if (++responses === n) {
          server.close();
        }
      });
    });
    req.id = i;
    requests++;
  }
});
