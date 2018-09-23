var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'lifecycle-initcwd')
var subdir = path.resolve(pkg, 'subdir')

var json = {
  name: 'init-cwd',
  version: '1.0.0',
  scripts: {
    initcwd: 'echo "$INIT_CWD"'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  mkdirp.sync(subdir)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(subdir)
  t.end()
})

test('make sure the env.INIT_CWD is correct', function (t) {
  common.npm(['run-script', 'initcwd'], {
    cwd: subdir
  }, function (er, code, stdout) {
    if (er) throw er
    t.equal(code, 0, 'exit code')
    stdout = stdout.trim().split(/\r|\n/).pop()
    var actual = stdout

    t.equal(actual, subdir)
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(subdir)
  rimraf.sync(pkg)
}
