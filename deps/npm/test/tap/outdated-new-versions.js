var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var path = require("path")

var mr = require("npm-registry-mock")

var pkg = path.resolve(__dirname, "outdated-new-versions")
var cache = path.resolve(pkg, "cache")
mkdirp.sync(cache)


test("dicovers new versions in outdated", function (t) {
  process.chdir(pkg)
  t.plan(2)

  mr({port : common.port}, function (er, s) {
    npm.load({cache: cache, registry: common.registry}, function () {
      npm.outdated(function (er, d) {
        for (var i = 0; i < d.length; i++) {
          if (d[i][1] === "underscore")
            t.equal("1.5.1", d[i][4])
          if (d[i][1] === "request")
            t.equal("2.27.0", d[i][4])
        }
        s.close()
        t.end()
      })
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(cache)
  t.end()
})
