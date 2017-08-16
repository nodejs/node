var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, path.basename(__filename, '.js'))

var json = {
  name: 'lifecycle-order',
  version: '1.0.0',
  scripts: {
    preinstall: 'node -e "var fs = require(\'fs\'); fs.openSync(\'preinstall-step\', \'w+\'); if (fs.existsSync(\'node_modules\')) { throw \'node_modules exists on preinstall\' }"',
    install: 'node -e "var fs = require(\'fs\'); if (fs.existsSync(\'preinstall-step\')) { fs.openSync(\'install-step\', \'w+\') } else { throw \'Out of order\' }"',
    postinstall: 'node -e "var fs = require(\'fs\'); if (fs.existsSync(\'install-step\')) { fs.openSync(\'postinstall-step\', \'w+\') } else { throw \'Out of order\' }"'
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

test('lifecycle scripts execute in the proper order', function (t) {
  common.npm('install', {cwd: pkg}, function (err, code, stdout, stderr) {
    if (err) throw err
    t.is(code, 0, 'no error')

    // All three files should exist
    t.ok(fs.existsSync(path.join(pkg, 'preinstall-step')), 'preinstall ok')
    t.ok(fs.existsSync(path.join(pkg, 'install-step')), 'install ok')
    t.ok(fs.existsSync(path.join(pkg, 'postinstall-step')), 'postinstall ok')

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
