var tap = require("tap")
var normalize = require("../lib/normalize")
var path = require("path")
var fs = require("fs")

tap.test("basic test", function (t) {
  var p = path.resolve(__dirname, "./fixtures/read-package-json.json")
  fs.readFile (p, function (err, contents) {
    if (err) throw err;
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
  t.equal(normalized._id, normalized.name + "@" + normalized.version, "It gets good id.")
  t.equal(normalized.name, original.name, "Name stays the same.")
  t.type(normalized.author, "object", "author field becomes object")
  t.deepEqual(normalized.scripts, original.scripts, "scripts field (object) stays same")
  t.equal(normalized.main, original.main)
  // optional deps are folded in.
  t.deepEqual(normalized.optionalDependencies,
              original.optionalDependencies)
  t.has(normalized.dependencies, original.optionalDependencies, "opt depedencies are copied into dependencies")
  t.has(normalized.dependencies, original.dependencies, "regular depedencies stay in place")
  t.deepEqual(normalized.devDependencies, original.devDependencies)
  t.type(normalized.bugs, "object", "bugs should become object")
  t.equal(normalized.bugs.url, "https://github.com/isaacs/read-package-json/issues")
}
