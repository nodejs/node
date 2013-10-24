var test = require("tap").test
var npm = require("../../")
var mkdirp = require("mkdirp")
var rimraf = require("rimraf")


var mr = require("npm-registry-mock")

// config
var port = 1331
var address = "http://localhost:" + port
var pkg = __dirname + "/outdated-new-versions"
mkdirp.sync(pkg + "/cache")


test("dicovers new versions in outdated", function (t) {
  process.chdir(pkg)
  t.plan(2)
  mr(port, function (s) {
    npm.load({cache: pkg + "/cache", registry: address}, function () {
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
  rimraf.sync(pkg + "/cache")
  t.end()
})
