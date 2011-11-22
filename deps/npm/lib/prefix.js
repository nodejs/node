module.exports = prefix

var npm = require("./npm.js")
  , output = require("./utils/output.js")

prefix.usage = "npm prefix\nnpm prefix -g\n(just prints the prefix folder)"

function prefix (args, cb) {
  output.write(npm.prefix, function (er) { cb(er, npm.prefix) })
}
