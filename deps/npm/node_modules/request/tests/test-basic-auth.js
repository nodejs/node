var assert = require('assert')
  , http = require('http')
  , request = require('../index')
  ;

var numBasicRequests = 0;

var basicServer = http.createServer(function (req, res) {
  console.error('Basic auth server: ', req.method, req.url);
  numBasicRequests++;

  var ok;

  if (req.headers.authorization) {
    if (req.headers.authorization == 'Basic ' + new Buffer('test:testing2').toString('base64')) {
      ok = true;
    } else {
      // Bad auth header, don't send back WWW-Authenticate header
      ok = false;
    }
  } else {
    // No auth header, send back WWW-Authenticate header
    ok = false;
    res.setHeader('www-authenticate', 'Basic realm="Private"');
  }

  if (ok) {
    console.log('request ok');
    res.end('ok');
  } else {
    console.log('status=401');
    res.statusCode = 401;
    res.end('401');
  }
});

basicServer.listen(6767);

request({
  'method': 'GET',
  'uri': 'http://localhost:6767/test/',
  'auth': {
    'user': 'test',
    'pass': 'testing2',
    'sendImmediately': false
  }
}, function(error, response, body) {
  assert.equal(response.statusCode, 200);
  assert.equal(numBasicRequests, 2);

  // If we don't set sendImmediately = false, request will send basic auth
  request({
    'method': 'GET',
    'uri': 'http://localhost:6767/test2/',
    'auth': {
      'user': 'test',
      'pass': 'testing2'
    }
  }, function(error, response, body) {
    assert.equal(response.statusCode, 200);
    assert.equal(numBasicRequests, 3);

    request({
      'method': 'GET',
      'uri': 'http://test:testing2@localhost:6767/test2/'
    }, function(error, response, body) {
      assert.equal(response.statusCode, 200);
      assert.equal(numBasicRequests, 4);

      console.log('All tests passed');
      basicServer.close();
    });
  });
});
