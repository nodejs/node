var fs = require('fs')
var resolve = require('path').resolve

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg
var target = resolve(pkg, '_target')

var EXEC_OPTS = {
  cwd: target
}

var json = {
  name: 'install-property-conflicts',
  version: '1.2.3',
  type: 'nose-boop!'
}

test('setup', function (t) {
  // make sure it installs locally
  mkdirp.sync(resolve(target, 'node_modules'))
  fs.writeFileSync(
    resolve(pkg, 'package.json'),
    JSON.stringify(json, null, 2) + '\n'
  )
  t.end()
})

test('install package with a `type` property', function (t) {
  t.comment('issue: https://github.com/npm/npm/issues/11398')
  common.npm(
    [
      'install',
      '--prefix', target,
      pkg
    ],
    EXEC_OPTS,
    function (err, code, stdout, stderr) {
      t.ifError(err, 'npm command ran from test')
      t.equals(code, 0, 'install exited with success (0)')
      var installedPkg = resolve(
        target,
        'node_modules',
        'install-property-conflicts',
        'package.json')
      t.ok(fs.statSync(installedPkg), 'package installed successfully')
      t.end()
    }
  )
})
