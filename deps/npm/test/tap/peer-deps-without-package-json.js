var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")

var http = require("http")


var js = new Buffer(
'/**package\n' +
' * { "name": "npm-test-peer-deps-file"\n' +
' * , "main": "index.js"\n' +
' * , "version": "1.2.3"\n' +
' * , "description":"No package.json in sight!"\n' +
' * , "peerDependencies": { "dict": "1.1.0" }\n' +
' * , "dependencies": { "opener": "1.3.0" }\n' +
' * }\n' +
' **/\n' +
'\n' +
'module.exports = "I\'m just a lonely index, naked as the day I was born."\n')

var server
test("setup", function(t) {
  server = http.createServer(function (q, s) {
    s.setHeader('content-type', 'application/javascript')
    s.end(js)
  })
  server.listen(1337, function() {
    t.pass('listening')
    t.end()
  })
})

test("installing a peerDependencies-using package without a package.json present (GH-3049)", function (t) {

  rimraf.sync(__dirname + "/peer-deps-without-package-json/node_modules")
  fs.mkdirSync(__dirname + "/peer-deps-without-package-json/node_modules")
  process.chdir(__dirname + "/peer-deps-without-package-json")

  npm.load(function () {
    npm.install('http://localhost:1337/', function (err) {
      if (err) {
        t.fail(err)
      } else {
        t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/npm-test-peer-deps-file"))
        t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/dict"))
      }
      t.end()
    })
  })
})

test("cleanup", function (t) {
  server.close(function() {
    t.pass("closed")
    t.end()
  })
})
