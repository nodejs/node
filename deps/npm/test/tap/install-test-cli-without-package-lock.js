var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-test-cli-without-package-lock',
  description: 'fixture',
  version: '0.0.0',
  dependencies: {
    dependency: 'file:./dependency'
  }
}

var dependency = {
  name: 'dependency',
  description: 'fixture',
  version: '0.0.0'
}

test('setup', function (t) {
  setup()
  t.pass('setup ran')
  t.end()
})

test('\'npm install-test\' should not generate package-lock.json.*', function (t) {
  common.npm(['install-test'], EXEC_OPTS, function (err, code, stderr, stdout) {
    if (err) throw err
    t.comment(stdout.trim())
    t.comment(stderr.trim())
    t.is(code, 0, 'npm install did not raise error code')
    var files = fs.readdirSync(pkg).filter(function (f) {
      return f.indexOf('package-lock.json.') === 0
    })
    t.notOk(
      files.length > 0,
      'package-lock.json.* should not be generated: ' + files
    )
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.pass('cleaned up')
  t.end()
})

function setup () {
  mkdirp.sync(path.join(pkg, 'dependency'))
  fs.writeFileSync(
    path.join(pkg, 'dependency', 'package.json'),
    JSON.stringify(dependency, null, 2)
  )

  mkdirp.sync(path.join(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  // Disable package-lock
  fs.writeFileSync(
    path.join(pkg, '.npmrc'),
    'package-lock=false\n'
  )
  process.chdir(pkg)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
