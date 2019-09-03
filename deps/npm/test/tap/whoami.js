var common = require('../common-tap.js')

var fs = require('fs')
var path = require('path')
var createServer = require('http').createServer

var test = require('tap').test
var rimraf = require('rimraf')

var opts = { cwd: __dirname }

var FIXTURE_PATH = path.resolve(common.pkg, 'fixture_npmrc')

test('npm whoami with basic auth', function (t) {
  var s = '//registry.lvh.me/:username = wombat\n' +
          '//registry.lvh.me/:_password = YmFkIHBhc3N3b3Jk\n' +
          '//registry.lvh.me/:email = lindsay@wdu.org.au\n'
  fs.writeFileSync(FIXTURE_PATH, s, 'ascii')
  fs.chmodSync(FIXTURE_PATH, 0o644)

  common.npm(
    [
      'whoami',
      '--userconfig=' + FIXTURE_PATH,
      '--registry=http://registry.lvh.me/'
    ],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)

      t.equal(stderr, '', 'got nothing on stderr')
      t.equal(code, 0, 'exit ok')
      t.equal(stdout, 'wombat\n', 'got username')
      t.end()
    }
  )
})

test('npm whoami with bearer auth', { timeout: 6000 }, function (t) {
  var s = '//localhost:' + common.port +
          '/:_authToken = wombat-developers-union\n'
  fs.writeFileSync(FIXTURE_PATH, s, 'ascii')
  fs.chmodSync(FIXTURE_PATH, 0o644)

  function verify (req, res) {
    t.equal(req.method, 'GET')
    t.equal(req.url, '/-/whoami')

    res.setHeader('content-type', 'application/json')
    res.writeHead(200)
    res.end(JSON.stringify({ username: 'wombat' }), 'utf8')
  }

  var server = createServer(verify)

  server.listen(common.port, function () {
    common.npm(
      [
        'whoami',
        '--userconfig=' + FIXTURE_PATH,
        '--registry=http://localhost:' + common.port + '/'
      ],
      opts,
      function (err, code, stdout, stderr) {
        t.ifError(err)

        t.equal(stderr, '', 'got nothing on stderr')
        t.equal(code, 0, 'exit ok')
        t.equal(stdout, 'wombat\n', 'got username')
        rimraf.sync(FIXTURE_PATH)
        server.close()
        t.end()
      }
    )
  })
})
