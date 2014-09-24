var test = require("tap").test
var npm = require("../../")
var path = require("path")
var fs = require("fs")
var rimraf = require("rimraf")
var pkg = path.join(__dirname, "install-save-local", "package")

test("setup", function (t) {
  resetPackageJSON(pkg)
  process.chdir(pkg)
  t.end()
})
test('"npm install --save ../local/path" should install local package and save to package.json', function(t) {
  resetPackageJSON(pkg)
  npm.load({loglevel : "silent"}, function() {
    npm.config.set("save", true)
    npm.commands.install(["../package-local-dependency"], function(err) {
      t.ifError(err)

      var dependencyPackageJson = path.resolve(pkg, "node_modules/package-local-dependency/package.json")
      t.ok(JSON.parse(fs.readFileSync(dependencyPackageJson, "utf8")))

      var pkgJson = JSON.parse(fs.readFileSync(pkg + "/package.json", "utf8"))
      t.deepEqual(pkgJson.dependencies, {
                  "package-local-dependency": "file:../package-local-dependency"
                })
      npm.config.set("save", undefined)

      t.end()
    })
  })
})

test('"npm install --save-dev ../local/path" should install local package and save to package.json', function(t) {
  resetPackageJSON(pkg)
  npm.load({loglevel : "silent"}, function() {
    npm.config.set("save-dev", true)
    npm.commands.install(["../package-local-dev-dependency"], function(err) {
      t.ifError(err)

      var dependencyPackageJson = path.resolve(pkg, "node_modules/package-local-dev-dependency/package.json")
      t.ok(JSON.parse(fs.readFileSync(dependencyPackageJson, "utf8")))

      var pkgJson = JSON.parse(fs.readFileSync(pkg + "/package.json", "utf8"))
      t.deepEqual(pkgJson.devDependencies, {
                  "package-local-dev-dependency": "file:../package-local-dev-dependency"
                })
      npm.config.set("save", undefined)

      t.end()
    })
  })
})

test("cleanup", function(t) {
  resetPackageJSON(pkg)
  process.chdir(__dirname)
  rimraf.sync(path.resolve(pkg, "node_modules"))
  t.end()
})

function resetPackageJSON(pkg) {
  var pkgJson = JSON.parse(fs.readFileSync(pkg + "/package.json", "utf8"))
  delete pkgJson.dependencies
  delete pkgJson.devDependencies
  var json = JSON.stringify(pkgJson, null, 2) + "\n"
  fs.writeFileSync(pkg + "/package.json", json, "ascii")
}
