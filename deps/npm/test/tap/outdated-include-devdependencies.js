var test = require("tap").test
var npm = require("../../")

var mr = require("npm-registry-mock")

// config
var port = 1331
var address = "http://localhost:" + port
var pkg = __dirname + '/outdated-include-devdependencies'


test("includes devDependencies in outdated", function (t) {
  process.chdir(pkg)
  mr(port, function (s) {
    npm.load({registry: address}, function () {
      npm.outdated(function (er, d) {
        t.equal("1.5.1", d[0][3])
        s.close()
        t.end()
      })
    })
  })
})