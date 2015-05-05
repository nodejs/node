var fs = require('graceful-fs')
var path = require('path')
var spawn = require('child_process').spawn

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var node = process.execPath
var npm = require.resolve('../../bin/npm-cli.js')

var pkg = path.resolve(__dirname, 'lifecycle-signal')

var json = {
  name: 'lifecycle-signal',
  version: '1.2.5',
  scripts: {
    preinstall: 'node -e "process.kill(process.pid,\'SIGSEGV\')"'
  }
}

test('setup', function (t) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('lifecycle signal abort', function (t) {
  // windows does not use lifecycle signals, abort
  if (process.platform === 'win32' || process.env.TRAVIS) return t.end()

  var child = spawn(node, [npm, 'install'], {
    cwd: pkg
  })
  child.on('close', function (code, signal) {
    t.equal(code, null)
    t.equal(signal, 'SIGSEGV')
    t.end()
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
