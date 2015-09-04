var resolve = require('path').resolve
var createReadStream = require('graceful-fs').createReadStream

var tap = require('tap')

var server = require('./lib/server.js')
var common = require('./lib/common.js')

var tgz = resolve(__dirname, './fixtures/underscore/1.3.3/package.tgz')

tap.test('fetch with a 404 response', function (t) {
  server.expect('/underscore/-/underscore-1.3.3.tgz', function (req, res) {
    t.equal(req.method, 'GET', 'got expected method')

    res.writeHead(404)

    createReadStream(tgz).pipe(res)
  })

  var client = common.freshClient()
  var defaulted = {}
  client.fetch(
    'http://localhost:1337/underscore/-/underscore-1.3.3.tgz',
    defaulted,
    function (err, res) {
      t.equal(
        err.message,
        'fetch failed with status code 404',
        'got expected error message'
      )
      t.end()
    }
  )
})
