var common = require("../common-tap.js")
var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")
var mr = require("npm-registry-mock")

// config
var pkg = __dirname + "/outdated-git"
mkdirp.sync(pkg + "/cache")


test("dicovers new versions in outdated", function (t) {
  process.chdir(pkg)
  t.plan(5)
  npm.load({cache: pkg + "/cache", registry: common.registry}, function () {
    npm.outdated(function (er, d) {
      t.equal('git', d[0][3])
      t.equal('git', d[0][4])
      t.equal('git://github.com/robertkowalski/foo-private.git', d[0][5])
      t.equal('git://user:pass@github.com/robertkowalski/foo-private.git', d[1][5])
      t.equal('git://github.com/robertkowalski/foo', d[2][5])
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(pkg + "/cache")
  t.end()
})
