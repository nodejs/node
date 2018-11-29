var fs = require('fs')
var path = require('path')

var mkdirp = require('mkdirp')
var mr = require('npm-registry-mock')
var test = require('tap').test

var npm = require('../../')

var common = require('../common-tap.js')
var pkg = common.pkg

var desired = {
  name: 'npm-test-shrinkwrap-prod-dependency',
  version: '0.0.0',
  dependencies: {
    request: {
      version: '0.9.0',
      resolved: common.registry + '/request/-/request-0.9.0.tgz',
      integrity: 'sha1-EEn1mm9GWI5tAwkh+7hMovDCcU4='
    },
    underscore: {
      dev: true,
      version: '1.5.1',
      resolved: common.registry + '/underscore/-/underscore-1.5.1.tgz',
      integrity: 'sha1-0r3oF9F2/63olKtxRY5oKhS4bck='
    }
  }
}

var json = {
  author: 'Domenic Denicola',
  name: 'npm-test-shrinkwrap-prod-dependency',
  version: '0.0.0',
  dependencies: {
    request: '0.9.0'
  },
  devDependencies: {
    underscore: '1.5.1'
  }
}

test('setup', function (t) {
  mkdirp.sync(pkg)
  fs.writeFileSync(path.join(pkg, 'package.json'), JSON.stringify(json, null, 2))
  process.chdir(pkg)

  var allOpts = {
    cache: common.cache,
    registry: common.registry
  }

  npm.load(allOpts, t.end)
})

test('mock registry', t => {
  mr({port: common.port}, function (er, s) {
    t.parent.teardown(() => s.close())
    t.end()
  })
})

test("shrinkwrap --dev doesn't strip out prod dependencies", t => {
  t.plan(1)
  npm.install('.', function (err) {
    if (err) return t.fail(err)

    npm.config.set('dev', true)
    npm.commands.shrinkwrap([], true, function (err, results) {
      if (err) return t.fail(err)

      t.deepEqual(results.dependencies, desired.dependencies)
      t.end()
    })
  })
})
