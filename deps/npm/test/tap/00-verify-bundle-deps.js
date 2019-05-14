var test = require('tap').test

var manifest = require('../../package.json')
var deps = Object.keys(manifest.dependencies)
var bundled = manifest.bundleDependencies

test('all deps are bundled deps or dev deps', function (t) {
  deps.forEach(function (name) {
    t.assert(
      bundled.indexOf(name) !== -1,
      name + ' is in bundledDependencies'
    )
  })

  t.end()
})
