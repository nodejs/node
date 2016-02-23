var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'logout')
var outfile = path.join(pkg, '_npmrc')
var opts = { cwd: pkg }

var contents = function () {/*
foo=boo
//localhost:1337/:_authToken=glarb
*/}.toString().split('\n').slice(1, -1).join('\n')

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
        '--loglevel', 'silent',
        '--userconfig', outfile
      ],
      opts,
      function (err, code) {
        t.ifError(err, 'no error output')
        t.notOk(code, 'exited OK')

        var config = fs.readFileSync(outfile, 'utf8')
        t.equal(config, 'foo=boo\n', 'creds gone')
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
