var resolve = require('path').resolve
var createReadStream = require('graceful-fs').createReadStream
var readFileSync = require('graceful-fs').readFileSync

var tap = require('tap')
var cat = require('concat-stream')

var server = require('./lib/server.js')
var common = require('./lib/common.js')

var tgz = resolve(__dirname, './fixtures/underscore/1.3.3/package.tgz')

tap.test('basic fetch with scoped always-auth enabled', function (t) {
  server.expect('/underscore/-/underscore-1.3.3.tgz', function (req, res) {
    t.equal(req.method, 'GET', 'got expected method')
    t.equal(
      req.headers.authorization,
      'Basic dXNlcm5hbWU6JTEyMzRAYXNkZiU=',
      'got expected auth header'
    )

    res.writeHead(200, {
      'content-type': 'application/x-tar',
      'content-encoding': 'gzip'
    })

    createReadStream(tgz).pipe(res)
  })

  var auth = {
    username: 'username',
    password: '%1234@asdf%',
    email: 'i@izs.me',
    alwaysAuth: true
  }

  var client = common.freshClient()
  var authed = { auth: auth }
  client.fetch(
    'http://localhost:1337/underscore/-/underscore-1.3.3.tgz',
    authed,
    function (er, res) {
      t.ifError(er, 'loaded successfully')

      var sink = cat(function (data) {
        t.deepEqual(data, readFileSync(tgz))
        t.end()
      })

      res.on('error', function (error) {
        t.ifError(error, 'no errors on stream')
      })

      res.pipe(sink)
    }
  )
})
