var fs = require('fs')
var path = require('path')

var mr = require('npm-registry-mock')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'install-save-prefix',
  version: '0.0.1'
}

test('start mock reg', function (t) {
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'started mock registry')
    t.parent.teardown(() => s.close())
    t.end()
  })
})

test('install --save with \'^\' save prefix should accept minor updates', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('run test', t => common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '^',
      '--save',
      'install', 'underscore@latest'
    ],
    EXEC_OPTS
  ).then(([code]) => {
    console.error('back from install!', code)
    t.equal(code, 0, 'npm install exited with code 0')

    var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
    t.ok(JSON.parse(fs.readFileSync(p)))

    var pkgJson = JSON.parse(fs.readFileSync(
      path.join(pkg, 'package.json'),
      'utf8'
    ))
    t.deepEqual(
      pkgJson.dependencies,
      { 'underscore': '^1.5.1' },
      'got expected save prefix and version of 1.5.1'
    )
  }))
})

test('install --save-dev with \'^\' save prefix should accept minor dev updates', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('run test', t => common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '^',
      '--save-dev',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS
  ).then(([code]) => {
    t.equal(code, 0, 'npm install exited with code 0')

    var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
    t.ok(JSON.parse(fs.readFileSync(p)))

    var pkgJson = JSON.parse(fs.readFileSync(
      path.join(pkg, 'package.json'),
      'utf8'
    ))
    t.deepEqual(
      pkgJson.devDependencies,
      { 'underscore': '^1.3.1' },
      'got expected save prefix and version of 1.3.1'
    )
    t.end()
  }))
})

test('install --save with \'~\' save prefix should accept patch updates', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('run test', t => common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '~',
      '--save',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS
  ).then(([code]) => {
    t.equal(code, 0, 'npm install exited with code 0')

    var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
    t.ok(JSON.parse(fs.readFileSync(p)))

    var pkgJson = JSON.parse(fs.readFileSync(
      path.join(pkg, 'package.json'),
      'utf8'
    ))
    t.deepEqual(
      pkgJson.dependencies,
      { 'underscore': '~1.3.1' },
      'got expected save prefix and version of 1.3.1'
    )
  }))
})

test('install --save-dev with \'~\' save prefix should accept patch updates', function (t) {
  t.plan(2)
  t.test('setup', setup)
  t.test('run test', t => common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '~',
      '--save-dev',
      'install', 'underscore@1.3.1'
    ],
    EXEC_OPTS
  ).then(([code]) => {
    t.notOk(code, 'npm install exited with code 0')

    var p = path.join(pkg, 'node_modules', 'underscore', 'package.json')
    t.ok(JSON.parse(fs.readFileSync(p)))

    var pkgJson = JSON.parse(fs.readFileSync(
      path.join(pkg, 'package.json'),
      'utf8'
    ))
    t.deepEqual(
      pkgJson.devDependencies,
      { 'underscore': '~1.3.1' },
      'got expected save prefix and version of 1.3.1'
    )
  }))
})

function setup (t) {
  t.test('destroy', t => {
    t.plan(2)
    rimraf(path.resolve(pkg, 'node_modules'), () => t.pass('node_modules'))
    rimraf(path.resolve(pkg, 'pacakage-lock.json'), () => t.pass('lock file'))
  })
  t.test('create', t => {
    fs.writeFileSync(
      path.join(pkg, 'package.json'),
      JSON.stringify(json, null, 2)
    )
    t.end()
  })

  t.end()
}
