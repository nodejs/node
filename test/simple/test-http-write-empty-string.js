var common = require('../common');
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

var response = '';

process.addListener('exit', function() {
  assert.equal('1\n2\n3\n', response);
});


server.listen(common.PORT, function() {
  var client = http.createClient(common.PORT);
  var req = client.request('/');
  req.end();
  req.addListener('response', function(res) {
    assert.equal(200, res.statusCode);
    res.setEncoding('ascii');
    res.addListener('data', function(chunk) {
      response += chunk;
    });
    common.error('Got /hello response');
  });
});

