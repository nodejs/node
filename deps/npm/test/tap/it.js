var join = require('path').join
var statSync = require('graceful-fs').statSync
var writeFileSync = require('graceful-fs').writeFileSync

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap')

var pkg = join(__dirname, 'run-script')
var installed = join(pkg, 'node_modules', 'underscore', 'package.json')

var json = {
  name: 'npm-it-test',
  dependencies: {
    underscore: '1.5.1'
  },
  scripts: {
    test: 'echo hax'
  }
}

var server

test('run up the mock registry', function (t) {
  mr({ port: common.port }, function (err, s) {
    if (err) throw err
    server = s
    t.end()
  })
})

test('npm install-test', function (t) {
  setup()
  common.npm(['install-test', '--registry=' + common.registry], { cwd: pkg }, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 0, 'command ran without error')
    t.ok(statSync(installed), 'package was installed')
    t.equal(require(installed).version, '1.5.1', 'underscore got installed as expected')
    t.match(stdout, /hax/, 'found expected test output')
    t.notOk(stderr, 'stderr should be empty')
    t.end()
  })
})

test('npm it (the form most people will use)', function (t) {
  setup()
  common.npm(['it', '--registry=' + common.registry], { cwd: pkg }, function (err, code, stdout, stderr) {
    if (err) throw err
    t.equal(code, 0, 'command ran without error')
    t.ok(statSync(installed), 'package was installed')
    t.equal(require(installed).version, '1.5.1', 'underscore got installed as expected')
    t.match(stdout, /hax/, 'found expected test output')
    t.notOk(stderr, 'stderr should be empty')
    t.end()
  })
})

test('cleanup', function (t) {
  process.chdir(osenv.tmpdir())
  server.close()
  cleanup()
  t.end()
})

function cleanup () {
  rimraf.sync(pkg)
}

function setup () {
  cleanup()
  mkdirp.sync(pkg)
  writeFileSync(join(pkg, 'package.json'), JSON.stringify(json, null, 2))
}
