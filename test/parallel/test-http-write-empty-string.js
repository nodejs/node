'use strict';
const common = require('../common');
var assert = require('assert');

var http = require('http');

var server = http.createServer(function(request, response) {
  console.log('responding to ' + request.url);

  response.writeHead(200, {'Content-Type': 'text/plain'});
  response.write('1\n');
  response.write('');
  response.write('2\n');
  response.write('');
  response.end('3\n');

  this.close();
});

server.listen(0, common.mustCall(function() {
  http.get({ port: this.address().port }, common.mustCall(function(res) {
    var response = '';

    assert.equal(200, res.statusCode);
    res.setEncoding('ascii');
    res.on('data', function(chunk) {
      response += chunk;
    });
    res.on('end', common.mustCall(function() {
      assert.strictEqual('1\n2\n3\n', response);
    }));
  }));
}));
