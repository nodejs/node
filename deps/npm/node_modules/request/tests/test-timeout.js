var server = require('./server')
  , events = require('events')
  , stream = require('stream')
  , assert = require('assert')
  , request = require('../index')
  ;

var s = server.createServer();
var expectedBody = "waited";
var remainingTests = 5;

s.listen(s.port, function () {
  // Request that waits for 200ms
  s.on('/timeout', function (req, resp) {
    setTimeout(function(){
      resp.writeHead(200, {'content-type':'text/plain'})
      resp.write(expectedBody)
      resp.end()
    }, 200);
  });

  // Scenario that should timeout
  var shouldTimeout = {
    url: s.url + "/timeout",
    timeout:100
  }


  request(shouldTimeout, function (err, resp, body) {
    assert.equal(err.code, "ETIMEDOUT");
    checkDone();
  })


  // Scenario that shouldn't timeout
  var shouldntTimeout = {
    url: s.url + "/timeout",
    timeout:300
  }

  request(shouldntTimeout, function (err, resp, body) {
    assert.equal(err, null);
    assert.equal(expectedBody, body)
    checkDone();
  })

  // Scenario with no timeout set, so shouldn't timeout
  var noTimeout = {
    url: s.url + "/timeout"
  }

  request(noTimeout, function (err, resp, body) {
    assert.equal(err);
    assert.equal(expectedBody, body)
    checkDone();
  })

  // Scenario with a negative timeout value, should be treated a zero or the minimum delay
  var negativeTimeout = {
    url: s.url + "/timeout",
    timeout:-1000
  }

  request(negativeTimeout, function (err, resp, body) {
    assert.equal(err.code, "ETIMEDOUT");
    checkDone();
  })

  // Scenario with a float timeout value, should be rounded by setTimeout anyway
  var floatTimeout = {
    url: s.url + "/timeout",
    timeout: 100.76
  }

  request(floatTimeout, function (err, resp, body) {
    assert.equal(err.code, "ETIMEDOUT");
    checkDone();
  })

  function checkDone() {
    if(--remainingTests == 0) {
      s.close();
      console.log("All tests passed.");
    }
  }
})

