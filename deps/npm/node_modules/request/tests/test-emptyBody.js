var request = require('../index')
  , http = require('http')
  , assert = require('assert')
  ;

var s = http.createServer(function (req, resp) {
  resp.statusCode = 200
  resp.end('')
}).listen(8080, function () {
  var r = request('http://localhost:8080', function (e, resp, body) {
    assert.equal(resp.statusCode, 200)
    assert.equal(body, "")

    var r2 = request({ url: 'http://localhost:8080', json: {} }, function (e, resp, body) {
	    assert.equal(resp.statusCode, 200)
	    assert.equal(body, undefined)
	    s.close()
 	 });
  })
})
