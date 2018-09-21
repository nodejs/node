var fs = require('fs')
var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var mkdirp = require('mkdirp')
var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'config-list')
var opts = { cwd: pkg, env: common.emptyEnv() }
var npmrc = path.resolve(pkg, '.npmrc')
var npmrcContents = `
_private=private;
registry/:_pwd=pwd;
foo=1234
`

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(pkg)

  // Write per-project conf file
  fs.writeFileSync(npmrc, npmrcContents, 'utf8')

  // Create empty package.json to indicate project root
  fs.writeFileSync(path.resolve(pkg, 'package.json'), '{}', 'utf8')
  t.end()
})

test('config list includes project config', function (t) {
  common.npm(
    ['config', 'list'],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)
      t.equal(stderr, '', 'stderr is empty')

      var expected = '; project config ' + npmrc + '\nfoo = "1234"'
      t.match(stdout, expected, 'contains project config')
      t.notMatch(stdout, '_private', 'excludes private config')
      t.notMatch(stdout, '_pwd', 'excludes private segmented config')
      t.end()
    }
  )
})

test('config list --json outputs json', function (t) {
  common.npm(
    ['config', 'list', '--json'],
    opts,
    function (err, code, stdout, stderr) {
      t.ifError(err)
      t.equal(stderr, '', 'stderr is empty')

      var json = JSON.parse(stdout)
      t.equal(json.foo, '1234', 'contains project config')
      t.equal(json.argv, undefined, 'excludes argv')
      t.equal(json._private, undefined, 'excludes private config')
      t.equal(json['registry/:_pwd'], undefined, 'excludes private config')
      t.end()
    }
  )
})

// TODO: test cases for other configuration types (cli, env, user, global)

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
