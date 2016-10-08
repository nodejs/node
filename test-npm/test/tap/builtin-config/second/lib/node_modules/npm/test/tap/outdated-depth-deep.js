var common = require('../common-tap')
var path = require('path')
var test = require('tap').test
var rimraf = require('rimraf')
var mr = require('npm-registry-mock')
var pkg = path.resolve(__dirname, 'outdated-depth-deep')
var cache = path.resolve(pkg, 'cache')

var osenv = require('osenv')
var mkdirp = require('mkdirp')
var fs = require('fs')

var pj = JSON.stringify({
  'name': 'whatever',
  'description': 'yeah idk',
  'version': '1.2.3',
  'main': 'index.js',
  'dependencies': {
    'underscore': '1.3.1',
    'npm-test-peer-deps': '0.0.0'
  },
  'repository': 'git://github.com/luk-/whatever'
}, null, 2)

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup () {
  mkdirp.sync(pkg)
  process.chdir(pkg)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), pj)
}

test('setup', function (t) {
  cleanup()
  setup()
  t.end()
})

test('outdated depth deep (9999)', function (t) {
  var conf = [
    '--registry', common.registry,
    '--cache', cache
  ]

  var server
  mr({ port: common.port }, thenTopLevelInstall)

  function thenTopLevelInstall (err, s) {
    if (err) throw err
    server = s
    common.npm(conf.concat(['install', '.']), {cwd: pkg}, thenDeepInstall)
  }

  function thenDeepInstall (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'install completed successfully')
    t.is('', stderr, 'no error output')
    var depPath = path.join(pkg, 'node_modules', 'npm-test-peer-deps')
    common.npm(conf.concat(['install', 'underscore']), {cwd: depPath}, thenRunOutdated)
  }

  function thenRunOutdated (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'deep install completed successfully')
    t.is('', stderr, 'no error output')
    common.npm(conf.concat(['outdated', '--depth', 9999]), {cwd: pkg}, thenValidateOutput)
  }

  function thenValidateOutput (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'outdated completed successfully')
    t.is('', stderr, 'no error output')
    t.match(
      stdout,
      /underscore.*1\.3\.1.*1\.3\.1.*1\.5\.1.*whatever\n/g,
      'child package listed')
    t.match(
      stdout,
      /underscore.*1\.3\.1.*1\.3\.1.*1\.5\.1.*whatever > npm-test-peer-deps/g,
      'child package listed')
    server.close()
    t.end()
  }
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})
