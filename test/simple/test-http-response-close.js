var common = require('../common');
var assert = require('assert');
var http = require('http');

var gotEnd = false;

var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.write('a');

  req.on('close', function() {
    console.error("aborted");
    gotEnd = true;
  });
});
server.listen(common.PORT);

server.addListener('listening', function() {
  console.error("make req");
  http.get({
    port: common.PORT
  }, function(res) {
    console.error("got res");
    res.on('data', function(data) {
      console.error("destroy res");
      res.destroy();
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.ok(gotEnd);
});
