var fs = require('fs')
var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'config-list')
var opts = { cwd: pkg }
var npmrc = path.resolve(pkg, '.npmrc')

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)
  t.end()
})

test('config list includes project config', function (t) {
  // Write per-project conf file
  fs.writeFileSync(npmrc, 'foo=1234', 'utf8')

  // Create empty package.json to indicate project root
  fs.writeFileSync(path.resolve(pkg, 'package.json'), '{}', 'utf8')

  common.npm(
    ['config', 'list'],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)
      t.equal(stderr, '', 'stderr is empty')
      var expected = '; project config ' + npmrc + '\nfoo = "1234"'
      t.match(stdout, expected, 'contains project config')
      t.end()
    }
  )
})

// TODO: test cases for other configuration types (cli, env, user, global)

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
