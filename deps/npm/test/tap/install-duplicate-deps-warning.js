var fs = require('graceful-fs')
var path = require('path')

var mr = require('npm-registry-mock')
var test = require('tap').test

var common = require('../common-tap.js')
var npm = require('../../')

var pkg = common.pkg

var json = {
  dependencies: {
    underscore: '1.5.1'
  },
  devDependencies: {
    underscore: '1.3.1'
  }
}

test('npm install with duplicate dependencies, different versions', function (t) {
  t.plan(1)
  t.comment('test for https://github.com/npm/npm/issues/6725')

  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(json, null, 2)
  )
  process.chdir(pkg)

  mr({ port: common.port }, function (er, s) {
    var opts = {
      cache: common.cache,
      registry: common.registry
    }

    npm.load(opts, function (err) {
      if (err) return t.fail(err)

      npm.install('.', function (err, additions, result) {
        if (err) return t.fail(err)

        var invalid = result.warnings.filter(function (warning) { return warning.code === 'EDUPLICATEDEP' })
        t.is(invalid.length, 1, 'got a warning (EDUPLICATEDEP) for duplicate dev/production dependencies')

        s.close()
        t.end()
      })
    })
  })
})
