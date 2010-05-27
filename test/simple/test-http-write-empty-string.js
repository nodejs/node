require('../common');

sys = require('sys');
http = require('http');
assert = require('assert');

server = http.createServer(function (request, response) {
  sys.puts('responding to ' + request.url);

  response.writeHead(200, {'Content-Type': 'text/plain'});
  response.write('1\n');
  response.write('');
  response.write('2\n');
  response.write('');
  response.end('3\n');

  this.close();
})
server.listen(PORT);

var response="";

process.addListener('exit', function () {
  assert.equal('1\n2\n3\n', response);
});


server.addListener('listening', function () {
  var client = http.createClient(PORT);
  var req = client.request("/");
  req.end();
  req.addListener('response', function (res) {
    assert.equal(200, res.statusCode);
    res.setEncoding("ascii");
    res.addListener('data', function (chunk) {
      response += chunk;
    });
    sys.error("Got /hello response");
  });
});

