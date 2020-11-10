var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')
var pkg = common.pkg
var local = path.join(pkg, 'package')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: '@scope/package',
  version: '0.0.0',
  peerDependencies: {
    underscore: '*'
  }
}

test('setup', function (t) {
  mkdirp.sync(local)
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(local, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  t.end()
})

test('it should install peerDependencies in same tree level as the parent package', function (t) {
  common.npm(['install', '--loglevel=warn', './package'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifError(err, 'install local package successful')
    t.equal(code, 0, 'npm install exited with code')
    t.match(stderr, /npm WARN @scope[/]package@0[.]0[.]0 requires a peer of underscore@[*] but none is installed[.] You must install peer dependencies yourself[.]\n/,
      'npm install warned about unresolved peer dep')

    t.end()
  })
})
