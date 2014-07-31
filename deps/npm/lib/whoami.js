module.exports = whoami

var npm = require("./npm.js")

whoami.usage = "npm whoami\n(just prints the 'username' config)"

function whoami (args, silent, cb) {
  if (typeof cb !== "function") cb = silent, silent = false
  var me = npm.config.get("username")
  var msg = me ? me : "Not authed.  Run 'npm adduser'"
  if (!silent) console.log(msg)
  process.nextTick(cb.bind(this, null, me))
}
