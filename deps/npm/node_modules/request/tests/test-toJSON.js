var request = require('../index')
  , http = require('http')
  , assert = require('assert')
  ;

var s = http.createServer(function (req, resp) {
  resp.statusCode = 200
  resp.end('asdf')
}).listen(8080, function () {
  var r = request('http://localhost:8080', function (e, resp) {
    assert.equal(JSON.parse(JSON.stringify(r)).response.statusCode, 200)
    s.close()
  })
})