var fs = require("fs")
var test = require("tap").test
var rimraf = require("rimraf")
var npm = require("../../")
var http = require("http")

var okFile = new Buffer(
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
'module.exports = "I\'m just a lonely index, naked as the day I was born."\n'
)

var failFile = new Buffer(
'/**package\n' +
' * { "name": "npm-test-peer-deps-file-invalid"\n' +
' * , "main": "index.js"\n' +
' * , "version": "1.2.3"\n' +
' * , "description":"This one should conflict with the other one"\n' +
' * , "peerDependencies": { "dict": "1.0.0" }\n' +
' * }\n' +
' **/\n' +
'\n' +
'module.exports = "I\'m just a lonely index, naked as the day I was born."\n'
)

var server
test("setup", function(t) {
  server = http.createServer(function (req, res) {
    res.setHeader('content-type', 'application/javascript')
    switch (req.url) {
      case "/ok.js":
        return res.end(okFile)
      default:
        return res.end(failFile)
    }
  })
  server.listen(1337, function() {
    t.pass("listening")
    t.end()
  })
})



test("installing dependencies that having conflicting peerDependencies", function (t) {
  rimraf.sync(__dirname + "/peer-deps-invalid/node_modules")
  process.chdir(__dirname + "/peer-deps-invalid")

  npm.load(function () {
    console.error('back from load')
    npm.commands.install([], function (err) {
      console.error('back from install')
      if (!err) {
        t.fail("No error!")
      } else {
        t.equal(err.code, "EPEERINVALID")
      }
      t.end()
    })
  })
})

test("shutdown", function(t) {
  server.close(function() {
    t.pass("closed")
    t.end()
  })
})
