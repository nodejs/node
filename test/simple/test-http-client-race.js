var common = require('../common');
var assert = require('assert');
var http = require('http');
var url = require('url');

var body1_s = '1111111111111111';
var body2_s = '22222';

var server = http.createServer(function(req, res) {
  var body = url.parse(req.url).pathname === '/1' ? body1_s : body2_s;
  res.writeHead(200,
                {'Content-Type': 'text/plain', 'Content-Length': body.length});
  res.end(body);
});
server.listen(common.PORT);

var body1 = '';
var body2 = '';

server.addListener('listening', function() {
  var client = http.createClient(common.PORT);

  var req1 = client.request('/1');
  req1.end();
  req1.addListener('response', function(res1) {
    res1.setEncoding('utf8');

    res1.addListener('data', function(chunk) {
      body1 += chunk;
    });

    res1.addListener('end', function() {
      var req2 = client.request('/2');
      req2.end();
      req2.addListener('response', function(res2) {
        res2.setEncoding('utf8');
        res2.addListener('data', function(chunk) { body2 += chunk; });
        res2.addListener('end', function() { server.close(); });
      });
    });
  });
});

process.addListener('exit', function() {
  assert.equal(body1_s, body1);
  assert.equal(body2_s, body2);
});
