var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var osenv = require('osenv')
var rimraf = require('rimraf')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = npm = require('../../')

var pkg = path.resolve(__dirname, 'shrinkwrap-shared-dev-dependency')

test("shrinkwrap doesn't strip out the shared dependency", function (t) {
  t.plan(1)

  mr({ port: common.port }, function (er, s) {
    setup(function (err) {
      if (err) return t.fail(err)

      npm.install('.', function (err) {
        if (err) return t.fail(err)
        npm.config.set('dev', true) // npm install unsets this

        npm.commands.shrinkwrap([], true, function (err, results) {
          if (err) return t.fail(err)

          t.deepEqual(results, desired)
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
  name: 'npm-test-shrinkwrap-shared-dev-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': {
      version: '0.0.0',
      from: 'test-package-with-one-dep@0.0.0',
      resolved: common.registry +
        '/test-package-with-one-dep/-/test-package-with-one-dep-0.0.0.tgz'
    },
    'test-package': {
      version: '0.0.0',
      from: 'test-package@0.0.0',
      resolved: common.registry + '/test-package/-/test-package-0.0.0.tgz'
    }
  }
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-shrinkwrap-shared-dev-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package-with-one-dep': '0.0.0'
  },
  devDependencies: {
    'test-package': '0.0.0'
  }
}

function setup (cb) {
  cleanup()
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
  process.chdir(pkg)

  var opts = {
    cache: path.resolve(pkg, 'cache'),
    registry: common.registry,
    // important to make sure devDependencies don't get stripped
    dev: true
  }
  npm.load(opts, cb)
}

function cleanup () {
  process.chdir(osenv.tmpdir())
  rimraf.sync(pkg)
}
