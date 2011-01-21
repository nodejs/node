var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.Server(function(req, res) {
  res.writeHead(200);
  res.end("hello world\n");
});

var responses = 0;
var N = 10;
var M = 10;

server.listen(common.PORT, function() {
  for (var i = 0; i < N; i++) {
    setTimeout(function () {
      for (var j = 0; j < M; j++) {
        http.get({ port: common.PORT, path: '/', }, function(res) {
          console.log(res.statusCode);
          if (++responses == N * M) server.close();
        }).on('error', function(e) {
          console.log(e.message);
          process.exit(1);
        });
      }
    }, i);
  }
});


process.on('exit', function() {
  assert.equal(N * M, responses);
});
