var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg
var subdir = path.resolve(pkg, 'subdir')

var json = {
  name: 'init-cwd',
  version: '1.0.0',
  scripts: {
    initcwd: process.platform === 'win32' ? 'echo %INIT_CWD%' : 'echo "$INIT_CWD"'
  }
}

test('setup', function (t) {
  mkdirp.sync(subdir)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  t.end()
})

test('make sure the env.INIT_CWD is correct', t =>
  common.npm(['run-script', 'initcwd'], { cwd: subdir })
    .then(([code, stdout, stderr]) => {
      t.equal(code, 0, 'exit code')
      stdout = stdout.trim().split(/\r|\n/).pop()
      var actual = stdout
      t.equal(actual, subdir)
    }))
