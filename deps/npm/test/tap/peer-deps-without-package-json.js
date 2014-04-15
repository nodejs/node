var common = require('../common-tap.js')
var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var mr = require("npm-registry-mock")
var http = require("http")


var js = new Buffer(
'/**package\n' +
' * { "name": "npm-test-peer-deps-file"\n' +
' * , "main": "index.js"\n' +
' * , "version": "1.2.3"\n' +
' * , "description":"No package.json in sight!"\n' +
' * , "peerDependencies": { "underscore": "1.3.1" }\n' +
' * , "dependencies": { "mkdirp": "0.3.5" }\n' +
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
  server.listen(common.port, function () {
    t.pass('listening')
    t.end()
  })
})

test("installing a peerDependencies-using package without a package.json present (GH-3049)", function (t) {

  rimraf.sync(__dirname + "/peer-deps-without-package-json/node_modules")
  fs.mkdirSync(__dirname + "/peer-deps-without-package-json/node_modules")
  process.chdir(__dirname + "/peer-deps-without-package-json")

  // we're already listening on common.port,
  // use an alternative port for this test.
  mr(1331, function (s) { // create mock registry.
    npm.load({registry: 'http://localhost:1331'}, function () {
      npm.install(common.registry, function (err) {
        if (err) {
          t.fail(err)
        } else {
          t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/npm-test-peer-deps-file"))
          t.ok(fs.existsSync(__dirname + "/peer-deps-without-package-json/node_modules/underscore"))
        }
        t.end()
        s.close() // shutdown mock registry.
      })
    })
  })
})

test("cleanup", function (t) {
  server.close(function() {
    t.pass("closed")
    t.end()
  })
})
