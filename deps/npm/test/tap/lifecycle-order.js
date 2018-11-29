var fs = require('graceful-fs')
var path = require('path')
var test = require('tap').test
var common = require('../common-tap.js')
var pkg = common.pkg

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
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  t.end()
})

test('lifecycle scripts execute in the proper order', t =>
  common.npm('install', {cwd: pkg}).then(([code, stdout, stderr]) => {
    t.is(code, 0, 'no error')

    // All three files should exist
    t.ok(fs.existsSync(path.join(pkg, 'preinstall-step')), 'preinstall ok')
    t.ok(fs.existsSync(path.join(pkg, 'install-step')), 'install ok')
    t.ok(fs.existsSync(path.join(pkg, 'postinstall-step')), 'postinstall ok')
  }))
