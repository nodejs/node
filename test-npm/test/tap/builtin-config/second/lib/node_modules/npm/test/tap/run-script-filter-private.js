var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = path.resolve(__dirname, 'run-script-filter-private')

var opts = { cwd: pkg }

var json = {
  name: 'run-script-filter-private',
  version: '1.2.3'
}

var npmrc = '//blah.com:_harsh=realms\n'

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.resolve(pkg, 'package.json'),
    JSON.stringify(json, null, 2) + '\n'
  )
  fs.writeFileSync(
    path.resolve(pkg, '.npmrc'),
    npmrc
  )
  t.end()
})

test('npm run-script env', function (t) {
  common.npm(['run-script', 'env'], opts, function (er, code, stdout, stderr) {
    t.ifError(er, 'using default env script')
    t.notOk(stderr, 'should not generate errors')
    t.ok(stdout.indexOf('npm_config_init_version') > 0, 'expected values in var list')
    t.notMatch(stdout, /harsh/, 'unexpected config not there')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}
