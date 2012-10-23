// vim: set softtabstop=16 shiftwidth=16:
var tap = require("tap")
var readJson = require("../")
var path = require("path")
var fs = require("fs")
var readme = fs.readFileSync(path.resolve(__dirname, "../README.md"), "utf8")
var package = require("../package.json")

console.error("basic test")
tap.test("basic test", function (t) {
                var p = path.resolve(__dirname, "../package.json")
                readJson(p, function (er, data) {
                                if (er) throw er;
                                basic_(t, data)
                })
})
function basic_ (t, data) {
                t.ok(data)
                t.equal(data.version, package.version)
                t.equal(data._id, data.name + "@" + data.version)
                t.equal(data.name, package.name)
                t.type(data.author, "object")
                t.equal(data.readme, readme)
                t.deepEqual(data.scripts, package.scripts)
                t.equal(data.main, package.main)
                t.equal(data.readmeFilename, 'README.md')

                // optional deps are folded in.
                t.deepEqual(data.optionalDependencies,
                            package.optionalDependencies)
                t.has(data.dependencies, package.optionalDependencies)
                t.has(data.dependencies, package.dependencies)

                t.deepEqual(data.devDependencies, package.devDependencies)
                t.end()
}
