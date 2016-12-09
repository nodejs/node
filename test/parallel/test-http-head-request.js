'use strict';
const common = require('../common');
var http = require('http');

var body = 'hello world\n';

function test(headers) {
  var server = http.createServer(function(req, res) {
    console.error('req: %s headers: %j', req.method, headers);
    res.writeHead(200, headers);
    res.end();
    server.close();
  });

  server.listen(0, common.mustCall(function() {
    var request = http.request({
      port: this.address().port,
      method: 'HEAD',
      path: '/'
    }, common.mustCall(function(response) {
      console.error('response start');
      response.on('end', common.mustCall(function() {
        console.error('response end');
      }));
      response.resume();
    }));
    request.end();
  }));
}

test({
  'Transfer-Encoding': 'chunked'
});
test({
  'Content-Length': body.length
});
