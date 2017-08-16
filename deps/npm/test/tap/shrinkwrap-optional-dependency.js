var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = npm = require('../../')

var pkg = path.resolve(__dirname, 'shrinkwrap-optional-dependency')

test('shrinkwrap does not fail on missing optional dependency', function (t) {
  t.plan(1)

  var mocks = {
    get: {
      '/random-package': [404, {}]
    }
  }

  mr({port: common.port, mocks: mocks}, function (er, s) {
    function fail (err) {
      s.close() // Close on failure to allow node to exit
      t.fail(err)
    }

    setup(function (err) {
      if (err) return fail(err)

      // Install without the optional dependency
      npm.install('.', function (err) {
        if (err) return fail(err)

        // Pretend the optional dependency was specified, but somehow failed to load:
        json.optionalDependencies = {
          'random-package': '0.0.0'
        }
        writePackage()

        npm.commands.shrinkwrap([], true, function (err, results) {
          if (err) return fail(err)

          t.deepEqual(results.dependencies, desired.dependencies)
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

var desired = {
  name: 'npm-test-shrinkwrap-optional-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package': {
      version: '0.0.0',
      resolved: common.registry + '/test-package/-/test-package-0.0.0.tgz',
      integrity: 'sha1-sNMrbEXCWcV4uiADdisgUTG9+9E='
    }
  }
}

var json = {
  author: 'Maximilian Antoni',
  name: 'npm-test-shrinkwrap-optional-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package': '0.0.0'
  }
}

function writePackage () {
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
}

function setup (cb) {
  cleanup()
  mkdirp.sync(pkg)
  writePackage()
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
