var common = require('../common');
var assert = require('assert');
var http = require('http');

var server = http.createServer(function(req, res) {
  console.log('got request. setting 1 second timeout');
  req.connection.setTimeout(500);

  req.connection.addListener('timeout', function() {
    common.debug('TIMEOUT');
    server.close();
  });
});

server.listen(common.PORT, function() {
  console.log('Server running at http://127.0.0.1:' + common.PORT + '/');

  var errorTimer = setTimeout(function() {
    throw new Error('Timeout was not sucessful');
  }, 2000);

  http.cat('http://localhost:' + common.PORT + '/', 'utf8',
           function(err, content) {
             clearTimeout(errorTimer);
             console.log('HTTP REQUEST COMPLETE (this is good)');
           });
});
