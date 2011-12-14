module.exports = substack
var npm = require("./npm.js")
  , log = require("./utils/log.js")

function substack (args, cb) {
  console.log("\033[32mbeep \033[35mboop\033[m")
  var c = args.shift()
  if (c) npm.commands[c](args, cb)
  else cb()
}
