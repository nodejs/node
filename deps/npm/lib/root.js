module.exports = root

var npm = require("./npm.js")
  , output = require("./utils/output.js")
  , log = require("./utils/log.js")

root.usage = "npm root\nnpm root -g\n(just prints the root folder)"

function root (args, cb) {
  output.write(npm.dir, function (er) { cb(er, npm.dir) })
}
