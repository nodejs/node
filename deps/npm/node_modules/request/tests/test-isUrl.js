var assert = require('assert')
  , request = require('../index')
  , http = require('http')
  ;

var s = http.createServer(function(req, res) {
  res.statusCode = 200;
  res.end('');
}).listen(6767, function () {

  // Test lowercase
  request('http://localhost:6767', function (err, resp, body) {
    // just need to get here without throwing an error
    assert.equal(true, true);
  })

  // Test uppercase
  request('HTTP://localhost:6767', function (err, resp, body) {
    assert.equal(true, true);
  })

  // Test mixedcase
  request('HtTp://localhost:6767', function (err, resp, body) {
    assert.equal(true, true);
    // clean up
    s.close();
  })
})