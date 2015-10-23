var test = require("tap").test

test("semver doc is up to date", function (t) {
  var path = require("path")
  var moddoc = path.join(__dirname, "../../node_modules/semver/README.md")
  var mydoc = path.join(__dirname, "../../doc/misc/semver.md")
  var fs = require("fs")
  var mod = fs.readFileSync(moddoc, "utf8").replace(/semver\(1\)/, "semver(7)")
  var my = fs.readFileSync(mydoc, "utf8")
  t.equal(my, mod)
  t.end()
})
