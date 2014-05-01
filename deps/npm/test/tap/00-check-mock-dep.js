console.log("TAP Version 13")

process.on("uncaughtException", function(er) {
  console.log("not ok - Failed checking mock registry dep. Expect much fail!")
  console.log("1..1")
  process.exit(1)
})

var assert = require("assert")
var semver = require("semver")
var mock = require("npm-registry-mock/package.json").version
var req = require("../../package.json").devDependencies["npm-registry-mock"]
assert(semver.satisfies(mock, req))
console.log("ok")
console.log("1..1")
