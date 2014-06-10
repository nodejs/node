var fs = require("fs")
var path = require("path")
var test = require("tap").test

var manifest = require("../../package.json")
var deps = Object.keys(manifest.dependencies)
var dev = Object.keys(manifest.devDependencies)
var bundled = manifest.bundleDependencies

test("all deps are bundled deps or dev deps", function (t) {
  deps.forEach(function (name) {
    t.assert(
      bundled.indexOf(name) !== -1,
      name + " is in bundledDependencies"
    )
  })

  t.same(
    fs.readdirSync(path.resolve(__dirname, '../../node_modules')).filter(function (name) {
      return (dev.indexOf(name) === -1) && (name !== ".bin")
    }).sort(),
    bundled.sort(),
    "bundleDependencies matches what's in node_modules"
  )

  t.end()
})
