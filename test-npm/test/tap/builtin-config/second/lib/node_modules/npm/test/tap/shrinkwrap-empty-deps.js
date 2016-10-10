var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = path.resolve(__dirname, 'shrinkwrap-empty-deps')

var EXEC_OPTS = { cwd: pkg }

var json = {
  author: 'Rockbert',
  name: 'shrinkwrap-empty-deps',
  version: '0.0.0',
  dependencies: {},
  devDependencies: {}
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

test('returns a list of removed items', function (t) {
  mr({ port: common.port }, function (er, s) {
    common.npm(
      [
        '--registry', common.registry,
        '--loglevel', 'silent',
        'shrinkwrap'
      ],
      EXEC_OPTS,
      function (err, code, stdout, stderr) {
        t.ifError(err, 'shrinkwrap ran without issue')
        t.notOk(code, 'shrinkwrap ran without raising error code')

        fs.readFile(path.resolve(pkg, 'npm-shrinkwrap.json'), function (err, desired) {
          t.ifError(err, 'read npm-shrinkwrap.json without issue')
          t.same(
            {
              'name': 'shrinkwrap-empty-deps',
              'version': '0.0.0'
            },
            JSON.parse(desired),
            'shrinkwrap handled empty deps without exploding'
          )

          s.close()
          t.end()
        })
      }
    )
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
