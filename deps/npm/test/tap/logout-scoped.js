var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg
var outfile = path.join(pkg, '_npmrc')
var opts = { cwd: pkg }

var contents = `foo=boo
@bar:registry=http://localhost:${common.port}
//localhost:${common.port}/:_authToken=glarb
`

function mocks (server) {
  server.delete('/-/user/token/glarb')
    .reply(200, {})
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('npm logout', function (t) {
  mr({ port: common.port, plugin: mocks }, function (err, s) {
    if (err) throw err

    common.npm(
      [
        'logout',
        '--registry', common.registry,
        '--scope', '@bar',
        '--loglevel', 'silent',
        '--userconfig', outfile
      ],
      opts,
      function (err, code) {
        t.ifError(err, 'no error output')
        t.notOk(code, 'exited OK')

        var config = fs.readFileSync(outfile, 'utf8')
        t.equal(config.trim(), 'foo=boo', 'creds gone')
        s.close()
        t.end()
      }
    )
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup () {
  mkdirp.sync(pkg)
  fs.writeFileSync(outfile, contents)
}

function cleanup () {
  rimraf.sync(pkg)
}
