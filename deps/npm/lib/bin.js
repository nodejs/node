module.exports = bin

var npm = require("./npm.js")
  , output = require("./utils/output.js")

bin.usage = "npm bin\nnpm bin -g\n(just prints the bin folder)"

function bin (args, cb) {
  var path = require("path")
    , b = npm.bin
    , PATH = (process.env.PATH || "").split(":")

  output.write(b, function (er) { cb(er, b) })

  if (npm.config.get("global") && PATH.indexOf(b) === -1) {
    output.write("(not in PATH env variable)"
                ,npm.config.get("logfd"))
  }
}
