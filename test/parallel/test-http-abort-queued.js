var assert = require('assert'),
    common = require('../common'),
    http = require('http');

var complete;

var server = http.createServer(function (req, res) {
  // We should not see the queued /thatotherone request within the server
  // as it should be aborted before it is sent.
  assert.equal(req.url, '/');

  res.writeHead(200);
  res.write('foo');

  complete = complete || function () {
    res.end();
  };
});


server.listen(common.PORT, function () {
  console.log('listen', server.address().port);

  var agent = new http.Agent({maxSockets: 1});
  assert.equal(Object.keys(agent.sockets).length, 0);

  var options = {
    hostname: 'localhost',
    port: server.address().port,
    method: 'GET',
    path: '/',
    agent: agent
  };

  var req1 = http.request(options);
  req1.on('response', function(res1) {
    assert.equal(Object.keys(agent.sockets).length, 1);
    assert.equal(Object.keys(agent.requests).length, 0);

    var req2 = http.request({
      method: 'GET',
      host: 'localhost',
      port: server.address().port,
      path: '/thatotherone',
      agent: agent
    });
    assert.equal(Object.keys(agent.sockets).length, 1);
    assert.equal(Object.keys(agent.requests).length, 1);

    req2.on('error', function(err) {
      // This is expected in response to our explicit abort call
      assert.equal(err.code, 'ECONNRESET');
    });

    req2.end();
    req2.abort();

    assert.equal(Object.keys(agent.sockets).length, 1);
    assert.equal(Object.keys(agent.requests).length, 1);

    console.log('Got res: ' + res1.statusCode);
    console.dir(res1.headers);

    res1.on('data', function(chunk) {
      console.log('Read ' + chunk.length + ' bytes');
      console.log(' chunk=%j', chunk.toString());
      complete();
    });

    res1.on('end', function() {
      console.log('Response ended.');

      setTimeout(function() {
        assert.equal(Object.keys(agent.sockets).length, 0);
        assert.equal(Object.keys(agent.requests).length, 0);

        server.close();
      }, 100);
    });
  });

  req1.end();
});
