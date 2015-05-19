'use strict';
var common = require('../common');
var assert = require('assert');

var http = require('http');

var request_count = 1000;
var body = '{"ok": true}';

var server = http.createServer(function(req, res) {
  res.writeHead(200, {'Content-Type': 'text/javascript'});
  res.write(body);
  res.end();
});
server.listen(common.PORT);

var requests_ok = 0;
var requests_complete = 0;

server.on('listening', function() {
  for (var i = 0; i < request_count; i++) {
    http.cat('http://localhost:' + common.PORT + '/', 'utf8',
             function(err, content) {
               requests_complete++;
               if (err) {
                 common.print('-');
               } else {
                 assert.equal(body, content);
                 common.print('.');
                 requests_ok++;
               }
               if (requests_complete == request_count) {
                 console.log('\nrequests ok: ' + requests_ok);
                 server.close();
               }
             });
  }
});

process.on('exit', function() {
  assert.equal(request_count, requests_complete);
  assert.equal(request_count, requests_ok);
});
