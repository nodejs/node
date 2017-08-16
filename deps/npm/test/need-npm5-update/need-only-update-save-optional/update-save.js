var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = path.resolve(__dirname, 'update-save')
var cache = path.resolve(pkg, 'cache')

var EXEC_OPTS = {
  cwd: pkg,
  stdio: 'ignore',
  env: {
    npm_config_registry: common.registry,
    npm_config_loglevel: 'verbose',
    npm_config_save_prefix: '^'
  }
}

var json = {
  name: 'update-save-example',
  version: '1.2.3',
  dependencies: {
    mkdirp: '~0.3.0'
  },
  devDependencies: {
    underscore: '~1.3.1'
  }
}

function clone (a) {
  return extend({}, a)
}

function extend (a, b) {
  for (var key in b) { a[key] = b[key] }
  return a
}

test('setup', function (t) {
  setup()

  mr({ port: common.port }, function (er, s) {
    t.ifError(er)
    server = s
    t.end()
  })
})

test('update regular dependencies only', function (t) {
  setup()

  common.npm(['update', '--save'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.notOk(code, 'npm update exited with code 0')

    var pkgdata = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'))
    t.deepEqual(
      pkgdata.dependencies,
      { mkdirp: '^0.3.5' },
      'only dependencies updated'
    )
    t.deepEqual(
      pkgdata.devDependencies,
      json.devDependencies,
      'dev dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.optionalDependencies,
      json.optionalDependencies,
      'optional dependencies should be untouched'
    )

    t.end()
  })
})

test('update devDependencies only', function (t) {
  setup()

  common.npm(['update', '--save-dev'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.notOk(code, 'npm update exited with code 0')

    var pkgdata = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'))
    t.deepEqual(
      pkgdata.dependencies,
      json.dependencies,
      'dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.devDependencies,
      { underscore: '^1.3.3' },
      'dev dependencies should be updated'
    )
    t.deepEqual(
      pkgdata.optionalDependencies,
      json.optionalDependencies,
      'optional dependencies should be untouched'
    )

    t.end()
  })
})

test('update optionalDependencies only', function (t) {
  setup({ optionalDependencies: { underscore: '~1.3.1' } })

  common.npm(['update', '--save-optional'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.notOk(code, 'npm update exited with code 0')

    var pkgdata = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'))
    t.deepEqual(
      pkgdata.dependencies,
      json.dependencies,
      'dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.devDependencies,
      json.devDependencies,
      'dev dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.optionalDependencies,
      { underscore: '^1.3.3' },
      'optional dependencies should be updated'
    )

    t.end()
  })
})

test('optionalDependencies are merged into dependencies during --save', function (t) {
  var cloned = setup({ optionalDependencies: { underscore: '~1.3.1' } })

  common.npm(['update', '--save'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.notOk(code, 'npm update exited with code 0')

    var pkgdata = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'))
    t.deepEqual(
      pkgdata.dependencies,
      { mkdirp: '^0.3.5' },
      'dependencies should not include optional dependencies'
    )
    t.deepEqual(
      pkgdata.devDependencies,
      cloned.devDependencies,
      'dev dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.optionalDependencies,
      cloned.optionalDependencies,
      'optional dependencies should be untouched'
    )

    t.end()
  })
})

test('semver prefix is replaced with configured save-prefix', function (t) {
  setup()

  common.npm(['update', '--save', '--save-prefix', '~'], EXEC_OPTS, function (err, code) {
    t.ifError(err)
    t.notOk(code, 'npm update exited with code 0')

    var pkgdata = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'))
    t.deepEqual(
      pkgdata.dependencies,
      { mkdirp: '~0.3.5' },
      'dependencies should be updated'
    )
    t.deepEqual(
      pkgdata.devDependencies,
      json.devDependencies,
      'dev dependencies should be untouched'
    )
    t.deepEqual(
      pkgdata.optionalDependencies,
      json.optionalDependencies,
      'optional dependencies should be updated'
    )

    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  cleanup()
  t.end()
})

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}

function setup (extendWith) {
  cleanup()
  mkdirp.sync(cache)
  process.chdir(pkg)

  var template = clone(json)
  extend(template, extendWith)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(template, null, 2)
  )
  return template
}
