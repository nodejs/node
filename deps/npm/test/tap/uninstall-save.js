var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var server

var pkg = path.join(__dirname, 'uninstall-save')

var EXEC_OPTS = { cwd: pkg }

var json = {
  name: 'uninstall-save',
  version: '0.0.1'
}

test('setup', function (t) {
  setup()
  mr({ port: common.port }, function (er, s) {
    t.ifError(er, 'started mock registry')
    server = s
    t.end()
  })
})

test('uninstall --save removes rm-ed package from package.json', function (t) {
  common.npm(
    [
      '--registry', common.registry,
      '--loglevel', 'silent',
      '--save-prefix', '^',
      '--save',
      'install', 'underscore@latest'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'npm install ran without issue')
      t.notOk(code, 'npm install exited with code 0')

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

      var installed = path.join(pkg, 'node_modules', 'underscore')
      rimraf.sync(installed)

      common.npm(
        [
          '--registry', common.registry,
          '--loglevel', 'debug',
          '--save',
          'uninstall', 'underscore'
        ],
        EXEC_OPTS,
        function (err, code) {
          t.ifError(err, 'npm uninstall ran without issue')

          var pkgJson = JSON.parse(fs.readFileSync(
            path.join(pkg, 'package.json'),
            'utf8'
          ))

          t.deepEqual(
            pkgJson.dependencies,
            { },
            'dependency removed as expected'
          )

          t.end()
        }
      )
    }
  )
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

function setup () {
  cleanup()
  mkdirp.sync(path.resolve(pkg, 'node_modules'))
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )

  process.chdir(pkg)
}
