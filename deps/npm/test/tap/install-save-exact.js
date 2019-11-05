var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-save-exact',
  version: '0.0.1',
  description: 'fixture'
}

test('mock registry', function (t) {
  mr({ port: common.port }, function (er, s) {
    t.parent.teardown(() => s.close())
    t.end()
  })
})

const setup = t => {
  t.test('destroy', t => rimraf(pkg, t.end))
  t.test('create', t => {
    mkdirp.sync(path.resolve(pkg, 'node_modules'))
    fs.writeFileSync(
      path.join(pkg, 'package.json'),
      JSON.stringify(json, null, 2)
    )
    t.end()
  })
  t.end()
}

const check = (savearg, deptype) => t => {
  common.npm(
    [
      '--loglevel', 'silent',
      '--registry', common.registry,
      savearg,
      '--save-exact',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm ran without issue')
      t.notOk(code, 'npm install exited without raising an error code')

      var p = path.resolve(pkg, 'node_modules/underscore/package.json')
      t.ok(JSON.parse(fs.readFileSync(p)))

      p = path.resolve(pkg, 'package.json')
      var pkgJson = JSON.parse(fs.readFileSync(p, 'utf8'))

      t.same(
        pkgJson[deptype],
        { 'underscore': '1.3.1' },
        'underscore dependency should specify exactly 1.3.1'
      )

      t.end()
    }
  )
}

test('\'npm install --save --save-exact\' should install local pkg', function (t) {
  t.test('setup', setup)
  t.test('check', check('--save', 'dependencies'))
  t.end()
})

test('\'npm install --save-dev --save-exact\' should install local pkg', function (t) {
  t.test('setup', setup)
  t.test('check', check('--save-dev', 'devDependencies'))
  t.end()
})
