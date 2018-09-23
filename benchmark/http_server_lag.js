var http = require('http');
var port = parseInt(process.env.PORT, 10) || 8000;
var defaultLag = parseInt(process.argv[2], 10) || 100;

http.createServer(function(req, res) {
  res.writeHead(200, { 'content-type': 'text/plain',
                       'content-length': '2' });

  var lag = parseInt(req.url.split("/").pop(), 10) || defaultLag;
  setTimeout(function() {
    res.end('ok');
  }, lag);
}).listen(port, 'localhost');
