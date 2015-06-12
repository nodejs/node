var fs = require('graceful-fs')
var http = require('http')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var pkg = path.join(__dirname, 'npm-test-unpublish-config')
var fixturePath = path.join(pkg, 'fixture_npmrc')

var common = require('../common-tap.js')

var json = {
  name: 'npm-test-unpublish-config',
  version: '1.2.3',
  publishConfig: { registry: common.registry }
}

test('setup', function (t) {
  mkdirp.sync(pkg)

  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json), 'utf8'
  )
  fs.writeFileSync(
    fixturePath,
    '//localhost:1337/:_authToken = beeeeeeeeeeeeef\n' +
      'registry = http://lvh.me:4321/registry/path\n'
  )

  t.end()
})

test('cursory test of unpublishing with config', function (t) {
  var child
  t.plan(4)
  http.createServer(function (req, res) {
    t.pass('got request on the fakey fake registry')
    this.close()
    res.statusCode = 500
    res.end(JSON.stringify({
      error: 'shh no tears, only dreams now'
    }))
    child.kill('SIGHUP')
  }).listen(common.port, function () {
    t.pass('server is listening')

    child = common.npm(
      [
        '--userconfig', fixturePath,
        '--loglevel', 'silent',
        '--force',
        'unpublish'
      ],
      {
        cwd: pkg,
        stdio: 'inherit',
        env: {
          'npm_config_cache_lock_stale': 1000,
          'npm_config_cache_lock_wait': 1000,
          HOME: process.env.HOME,
          Path: process.env.PATH,
          PATH: process.env.PATH,
          USERPROFILE: osenv.home()
        }
      },
      function (err, code) {
        t.ifError(err, 'publish command finished successfully')
        t.notOk(code, 'npm install exited with code 0')
      }
    )
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
  t.end()
})
