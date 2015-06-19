var tap = require("tap")
var normalize = require("../lib/normalize")
var path = require("path")
var fs = require("fs")

tap.test("bad scripts", function (t) {
  var p = path.resolve(__dirname, "./fixtures/badscripts.json")
  fs.readFile (p, function (err, contents) {
    if (err) throw err
    var originalData = JSON.parse(contents.toString())
    var data = JSON.parse(contents.toString())
    normalize(data)
    t.ok(data)
    verifyFields(t, data, originalData)
    t.end()
  })
})

function verifyFields (t, normalized, original) {
  t.equal(normalized.version, original.version, "Version field stays same")
  t.equal(normalized.name, original.name, "Name stays the same.")
  // scripts is not an object, so it should be deleted
  t.notOk(normalized.scripts)
}
