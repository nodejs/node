var tap = require('tap')

var server = require('./lib/server.js')
var common = require('./lib/common.js')

tap.test('get returns 403', function (t) {
  server.expect('/underscore', function (req, res) {
    t.equal(req.method, 'GET', 'got expected method')

    res.writeHead(403)
    res.end(JSON.stringify({
      error: 'get that cat out of the toilet that\'s gross omg'
    }))
  })

  var client = common.freshClient()
  client.get(
    'http://localhost:1337/underscore',
    {},
    function (er) {
      t.ok(er, 'failed as expected')

      t.equal(er.statusCode, 403, 'status code was attached to error as expected')
      t.equal(er.code, 'E403', 'error code was formatted as expected')

      t.end()
    }
  )
})
