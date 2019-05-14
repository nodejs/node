var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../')

var pkg = path.resolve(__dirname, 'dev-dep-duplicate')

var json = {
  author: 'Anders Janmyr',
  name: 'dev-dep-duplicate',
  version: '0.0.0',
  dependencies: {
    underscore: '1.5.1'
  },
  devDependencies: {
    underscore: '1.3.1'
  }
}

var expected = {
  name: 'dev-dep-duplicate',
  version: '0.0.0',
  dependencies: {
    underscore: {
      version: '1.5.1',
      from: 'underscore@1.5.1',
      resolved: common.registry + '/underscore/-/underscore-1.5.1.tgz',
      invalid: true
    }
  }
}

test('prefers version from dependencies over devDependencies', function (t) {
  t.plan(1)

  mr({ port: common.port }, function (er, s) {
    setup(function (err) {
      if (err) return t.fail(err)

      npm.install('.', function (err) {
        if (err) return t.fail(err)

        npm.commands.ls([], true, function (err, _, results) {
          if (err) return t.fail(err)

          // these contain full paths so we can't do an exact match
          // with them
          delete results.problems
          delete results.dependencies.underscore.problems
          t.deepEqual(results, expected)
          s.close()
          t.end()
        })
      })
    })
  })
})

test('cleanup', function (t) {
  cleanup()
  t.end()
})

function setup (cb) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)

  var opts = {
    cache: path.resolve(pkg, 'cache'),
    registry: common.registry
  }
  npm.load(opts, cb)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
