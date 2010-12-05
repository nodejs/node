var common = require('../common');
var assert = require('assert');
var http = require('http');

var body = 'exports.A = function() { return "A";}';
var server = http.createServer(function(req, res) {
  console.log('got request');
  res.writeHead(200, [
    ['Content-Length', body.length],
    ['Content-Type', 'text/plain']
  ]);
  res.end(body);
});

var got_good_server_content = false;
var bad_server_got_error = false;

server.listen(common.PORT, function() {
  http.cat('http://localhost:' + common.PORT + '/', 'utf8',
           function(err, content) {
             if (err) {
               throw err;
             } else {
               console.log('got response');
               got_good_server_content = true;
               assert.equal(body, content);
               server.close();
             }
           });

  http.cat('http://localhost:12312/', 'utf8', function(err, content) {
    if (err) {
      console.log('got error (this should happen)');
      bad_server_got_error = true;
    }
  });
});

process.addListener('exit', function() {
  console.log('exit');
  assert.equal(true, got_good_server_content);
  assert.equal(true, bad_server_got_error);
});
