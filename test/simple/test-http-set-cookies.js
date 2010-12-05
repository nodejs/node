var common = require('../common');
var assert = require('assert');
var http = require('http');

var nresponses = 0;

var server = http.createServer(function(req, res) {
  if (req.url == '/one') {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['content-type', 'text/plain']]);
    res.end('one\n');
  } else {
    res.writeHead(200, [['set-cookie', 'A'],
                        ['set-cookie', 'B'],
                        ['content-type', 'text/plain']]);
    res.end('two\n');
  }
});
server.listen(common.PORT);

server.addListener('listening', function() {
  //
  // one set-cookie header
  //
  var client = http.createClient(common.PORT);
  var req = client.request('GET', '/one');
  req.end();

  req.addListener('response', function(res) {
    // set-cookie headers are always return in an array.
    // even if there is only one.
    assert.deepEqual(['A'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.addListener('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.addListener('end', function() {
      if (++nresponses == 2) {
        server.close();
      }
    });
  });

  // two set-cookie headers

  var client = http.createClient(common.PORT);
  var req = client.request('GET', '/two');
  req.end();

  req.addListener('response', function(res) {
    assert.deepEqual(['A', 'B'], res.headers['set-cookie']);
    assert.equal('text/plain', res.headers['content-type']);

    res.addListener('data', function(chunk) {
      console.log(chunk.toString());
    });

    res.addListener('end', function() {
      if (++nresponses == 2) {
        server.close();
      }
    });
  });

});

process.addListener('exit', function() {
  assert.equal(2, nresponses);
});
