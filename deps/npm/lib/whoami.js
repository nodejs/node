module.exports = whoami

var npm = require("./npm.js")
  , output = require("./utils/output.js")

whoami.usage = "npm whoami\n(just prints the 'username' config)"

function whoami (args, cb) {
  var me = npm.config.get("username")
  if (!me) me = "Not authed.  Run 'npm adduser'"
  output.write(me, cb)
}
