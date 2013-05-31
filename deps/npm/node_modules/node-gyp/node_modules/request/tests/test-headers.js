var server = require('./server')
  , assert = require('assert')
  , request = require('../main.js')
  , Cookie = require('../vendor/cookie')
  , Jar = require('../vendor/cookie/jar')
  , s = server.createServer()

s.listen(s.port, function () {
  var serverUri = 'http://localhost:' + s.port
    , numTests = 0
    , numOutstandingTests = 0

  function createTest(requestObj, serverAssertFn) {
    var testNumber = numTests;
    numTests += 1;
    numOutstandingTests += 1;
    s.on('/' + testNumber, function (req, res) {
      serverAssertFn(req, res);
      res.writeHead(200);
      res.end();
    });
    requestObj.url = serverUri + '/' + testNumber
    request(requestObj, function (err, res, body) {
      assert.ok(!err)
      assert.equal(res.statusCode, 200)
      numOutstandingTests -= 1
      if (numOutstandingTests === 0) {
        console.log(numTests + ' tests passed.')
        s.close()
      }
    })
  }

  // Issue #125: headers.cookie shouldn't be replaced when a cookie jar isn't specified
  createTest({headers: {cookie: 'foo=bar'}}, function (req, res) {
    assert.ok(req.headers.cookie)
    assert.equal(req.headers.cookie, 'foo=bar')
  })

  // Issue #125: headers.cookie + cookie jar
  var jar = new Jar()
  jar.add(new Cookie('quux=baz'));
  createTest({jar: jar, headers: {cookie: 'foo=bar'}}, function (req, res) {
    assert.ok(req.headers.cookie)
    assert.equal(req.headers.cookie, 'foo=bar; quux=baz')
  })

  // There should be no cookie header when neither headers.cookie nor a cookie jar is specified
  createTest({}, function (req, res) {
    assert.ok(!req.headers.cookie)
  })
})
