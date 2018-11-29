var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var t = require('tap')

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var PACKAGE_JSON1 = {
  name: 'install-cli-production-nosave',
  version: '0.0.1',
  dependencies: {
  }
}

t.test('setup', function (t) {
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(PACKAGE_JSON1, null, 2)
  )
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'started mock registry')
    t.parent.teardown(() => s.close())
    t.end()
  })
})

t.test('install --production <module> without --save exits successfully', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      'install', '--production', 'underscore'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')
      t.end()
    }
  )
})
